/*
 * ha_nlink.c
 *
 * Created: 5/1/2018 11:18:00 PM
 *  Author: Solit
 */ 

#include <avr/interrupt.h>
#include <stdint.h>
#include <string.h>
#include <avr/sfr_defs.h>
#include "ha-common.h"
#include "ha-nlink.h"

nlink_t nlink;

#define NODE_FLAG_VALID 1      // for sanity purposes
#define NODE_FLAG_RX    2      // 

#define NLINK_RX_INT_ENABLE do { GICR |= _BV(INT0); GIFR |= INTF0; } while(0)
#define NLINK_RX_INT_DISABLE GICR &= ~_BV(INT0)

#define NLINK_IO_IDLE_TIMEOUT 16    // timeout when TX can start to transmit. 
// TODO: ^^^ Must be unique for a particular device

#define NLINK_IO_TIMER_ENABLE  TIMSK |= _BV(TOIE2)
#define NLINK_IO_TIMER_DISABLE TIMSK &= ~_BV(TOIE2)
#define NLINK_IO_TIMER_RELOAD  (0xFF - 255)        // 0 - free running

static void ha_nlink_io_set_idle()
{
    nlink.io.state = NLINK_IO_STATE_IDLE;
    nlink.io.idle_timer = 0;
    NLINK_RX_INT_ENABLE;
}
static void ha_nlink_io_recover()
{
    NLINK_IO_TX_PORT &= ~NLINK_IO_TX_PIN_MASK;
    nlink.io.state = NLINK_IO_STATE_RECOVERING;
    nlink.io.recover_timer = NLINK_IO_RECOVER_TIMER;
    nlink.io.rx_wr = 0;
    NLINK_RX_INT_ENABLE;        // Recovery timer will be restarted on pin fall
}

static void ha_nlink_io_init_timer()
{   // Timer is always running
    TCNT2 = NLINK_IO_TIMER_RELOAD;
    TIFR |= _BV(TOV2);
    TIMSK |= _BV(TOIE2);

    TCCR2 &= ~(_BV(CS20) | _BV(CS21) | _BV(CS22));
    TCCR2 |= (2 << CS20);    // prescaler 2 => 1/8
}

void ha_nlink_init()
{
    memset((uint8_t*)&nlink, 0, sizeof(nlink_t));
    for (uint8_t i = 0; i < NLINK_NODES_NUM; i ++) {
        nlink.nodes[i].idx = i;
    }

    NLINK_IO_RX_PORT |= NLINK_IO_RX_PIN_MASK;
    NLINK_IO_RX_DIR  &= ~NLINK_IO_RX_PIN_MASK;   // pull-up input

    PORTA |= PINA1;
    DDRA &= ~PINA1;

    NLINK_IO_TX_PORT &= ~NLINK_IO_TX_PIN_MASK;
    NLINK_IO_TX_DIR  |= NLINK_IO_TX_PIN_MASK;   // output

    // Set INT0 trigger to Falling Edge, Clear interrupt flag
    MCUCR &= ~(_BV(ISC00) | _BV(ISC01));
    MCUCR |= (2 << ISC00);
    
    ha_nlink_io_recover();
    ha_nlink_io_init_timer();
}

node_t* ha_nlink_node_register(uint8_t addr, uint8_t type, node_rx_cb_t on_rx_cb) 
{
    // Find next free entry
    int8_t i;
    node_t *node;
    for (i = 0; i < NLINK_NODES_NUM; i++){
        node = &nlink.nodes[i];
        if (node->addr == 0) break;             // Empty entry found
    }

    if (i == NLINK_NODES_NUM) return NULL;      // Empty entry not found

    node->addr = addr;
    node->type = type;
    node->on_rx_cb = on_rx_cb;
    return node;
}

uint8_t nlink_node_on_rx(uint8_t *rx_buf)
{
    uint8_t consumed = 0;
    uint8_t addr_to = rx_buf[NLINK_HDR_OFF_TO];

    // TODO: check most efficient loop (while + pointer; for + index; etc)
    //       node++ in loop - worst case
    //       
    for (uint8_t i = 0; i < NLINK_NODES_NUM; i++) {
        node_t *node = &nlink.nodes[i];
        if (node->addr == 0) break;
        if (node->addr == rx_buf[NLINK_HDR_OFF_FROM]) continue;

        if ( node->addr == addr_to ||               // Direct message
             node->type == NODE_TYPE_CTRLCON ||     // CTRLCON listens for all messages
             addr_to == NODE_ADDR_BC ) {            // Broadcast message
            // Address matched
            consumed |= (node->addr == addr_to);

            if (rx_buf[NLINK_HDR_OFF_CMD] == NLINK_CMD_RD_REQ) {
                // RD_REQ doesn't require node involving just send node's TX buffer
                ha_nlink_node_send(node, rx_buf[NLINK_HDR_OFF_FROM], NLINK_CMD_RD_RESP);
            } else {
                node->on_rx_cb(node->idx, rx_buf);
            }
        }
    }

    return consumed;
}

void ha_nlink_node_send(node_t *node, uint8_t addr_to, uint8_t cmd) 
{
    // node_t *node = &nlink.nodes[node_h];
    node->tx_buf[NLINK_HDR_OFF_FROM] = node->addr;
    node->tx_buf[NLINK_HDR_OFF_TO  ] = addr_to;
    node->tx_buf[NLINK_HDR_OFF_CMD ] = cmd;
                 
    // Check Local nodes
    uint8_t consumed = nlink_node_on_rx(node->tx_buf);

    // Mark as "to be send outward" if not consumed
    node->tx_flag = !consumed;
}

void ha_nlink_check_rx() 
{
    if (nlink.io.state != NLINK_IO_STATE_IDLE) 
        return;

    // Data can be read after 1 clock in idle state.
    // This is because TX should invalidate RX data after check
    if (nlink.io.idle_timer < 1)
        return;

    uint8_t rx_wr = nlink.io.rx_wr;

    // Check is full message received
    if ( rx_wr > NLINK_HDR_OFF_LEN &&
         rx_wr == nlink.io.rx_buf[NLINK_HDR_OFF_LEN] + NLINK_HDR_OFF_DATA) { 

        uint8_t rx_buf[NLINK_COMM_BUF_SIZE];
        
        // Make local buffer copy and reset write index
        memcpy(rx_buf, nlink.io.rx_buf, rx_wr - 1);
        nlink.io.rx_wr = 0;

        nlink_node_on_rx(rx_buf);
    }
}

void ha_nlink_check_tx() 
{
    if (nlink.io.state != NLINK_IO_STATE_IDLE || 
        nlink.io.idle_timer != NLINK_IO_IDLE_TIMEOUT) {
        return;
    }

    // IO Idle timeout expired
    // Check pending transmittion (in case of previous one failed)
    if (nlink.io.tx_len) {
        nlink.io.tx_rd = 0;
        return;
    } 
    // Get data to transfer from nodes
    for(uint8_t i = 0; i < ARRAY_SIZE(nlink.nodes); i++) {
        node_t *node = &nlink.nodes[i];
        if (node->tx_flag) {
            node->tx_flag = 0;
            uint8_t tx_len = node->tx_buf[NLINK_HDR_OFF_LEN] + NLINK_HDR_OFF_DATA;
            memcpy(nlink.io.tx_buf, node->tx_buf, tx_len);
            // Initiate transmittion in next timer interrupt
            cli();
                nlink.io.tx_len = tx_len;
                nlink.io.tx_rd = 0;
            sei();
            return;
        }
    }
}

static void isr_nlink_io_rx_on_idle() 
{
    if (nlink.io.idle_timer < NLINK_IO_IDLE_TIMEOUT) {
        nlink.io.idle_timer++;
    }
}
static void isr_nlink_io_rx_on_receiving() {

    //_______      _______ _     __ _________ _____
    //   |   \___/\___0___X_ ... __X____7____/
    //   |     |             |                  |
    //   |     |             |                  |
    // Idle   Start       Data 8bits          Stop
    
    uint8_t bit_in = !!(NLINK_IO_RX_PIN & NLINK_IO_RX_PIN_MASK);
    uint8_t bit_cnt = nlink.io.bit_cnt;

    nlink.io.bit_cnt++;

    if (bit_cnt == 0) {
        // Check START bit
        if (bit_in) {
            ha_nlink_io_recover();
        }
        return;
    }

    if (bit_cnt == 9) {
        // Check STOP bit
        if (!bit_in) {
            ha_nlink_io_recover();
        } else {
            if (nlink.io.rx_wr == sizeof(nlink.io.rx_buf)) {
                ha_nlink_io_recover();
            } else {
                nlink.io.rx_buf[nlink.io.rx_wr++] = nlink.io.rx_shift_reg;
                ha_nlink_io_set_idle(); // Wait for next byte start
            }
        }
        return;
    }

    nlink.io.rx_shift_reg = (nlink.io.rx_shift_reg >> 1) | (bit_in << 7);
    
}

static void isr_nlink_io_on_tx_timer()
{
    if (nlink.io.tx_len == 0) {
        return;
    }

    switch(nlink.io.state) {
        case NLINK_IO_STATE_ACTIVE:
            if (nlink.io.bit_cnt > 0) {
                if ( (nlink.io.bit_cnt == 9) || (nlink.io.tx_shift_reg & 1)) {
                    // Stop bit or 1-data bit
                    NLINK_IO_TX_PORT &= ~NLINK_IO_TX_PIN_MASK;
                } else {
                    // 0-data bit
                    NLINK_IO_TX_PORT |= NLINK_IO_TX_PIN_MASK;
                }
            } 
            nlink.io.tx_shift_reg >>= 1;
            break;

        case NLINK_IO_STATE_IDLE:
            if (nlink.io.tx_rd < nlink.io.tx_len) {
                // Something to send
                nlink.io.tx_shift_reg = nlink.io.tx_buf[nlink.io.tx_rd++];
                // Start bit
                NLINK_IO_TX_PORT |= NLINK_IO_TX_PIN_MASK;
            } else if (nlink.io.tx_rd == nlink.io.tx_len) {
                // Check that transmitted header equals to received
                if ( nlink.io.rx_buf[0] == nlink.io.tx_buf[0] &&
                     nlink.io.rx_buf[1] == nlink.io.tx_buf[1]) {
                    // Transmittion OK
                    nlink.io.tx_rd = 0;
                    nlink.io.tx_len = 0;
                } else {
                    // Transmittion failed. 
                    // Use tx_rd > tx_len as retransmit required flag
                    // Retransmittion will be initiated in idle loop 
                    // after idle timeout expired
                    nlink.io.tx_rd = 0xFF;   
                }
                // Invalidate just received own data
                nlink.io.rx_wr = 0;
            }
            break;
        case NLINK_IO_STATE_RECOVERING:
            break;
        default:
            FATAL_TRAP(__LINE__);
    }
}

static void isr_nlink_io_on_rx_timer()
{
    switch(nlink.io.state) {
        case NLINK_IO_STATE_ACTIVE:
            isr_nlink_io_rx_on_receiving();
            break;
        case NLINK_IO_STATE_RECOVERING:
            // Reload timer if pin still in 0
            if (0 == (NLINK_IO_RX_PIN & NLINK_IO_RX_PIN_MASK)) {
                nlink.io.recover_timer = NLINK_IO_RECOVER_TIMER;
            }
            if (--nlink.io.recover_timer == 0) {
                ha_nlink_io_set_idle();
            }
            break;

        case NLINK_IO_STATE_IDLE:
            isr_nlink_io_rx_on_idle();
            break;
        default:
            FATAL_TRAP(__LINE__);
    }
}

static void isr_nlink_io_on_timer()
{
    if (nlink.io.is_rx_timer) {
        isr_nlink_io_on_rx_timer();
    } else {
        isr_nlink_io_on_tx_timer();
    }
    nlink.io.is_rx_timer = !nlink.io.is_rx_timer;
}

ISR(TIMER2_OVF_vect)
{
    TCNT2 = NLINK_IO_TIMER_RELOAD;
    // 8*256 clocks @ 8MHz = 8*64us = 256us = 1/2 nlink clock period
    isr_nlink_io_on_timer();
}

ISR(INT0_vect)
{
    // Triggered on RX falling edge

    if (nlink.io.state == NLINK_IO_STATE_RECOVERING) {
        nlink.io.recover_timer = NLINK_IO_RECOVER_TIMER;
        return;
    }
    
    NLINK_RX_INT_DISABLE;
    nlink.io.is_rx_timer = 1;      // Next timer interrupt will be RX
//    nlink.io.shift_reg = 0;
    nlink.io.bit_cnt = 0;
    nlink.io.state = NLINK_IO_STATE_ACTIVE;

    TIFR |= _BV(TOV2);
    TCNT2 = NLINK_IO_TIMER_RELOAD + 1;   // Enable timer
}
