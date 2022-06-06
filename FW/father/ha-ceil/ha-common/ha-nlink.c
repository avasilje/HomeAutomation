/*
 * ha_nlink.c
 *
 * Created: 5/1/2018 11:18:00 PM
 *  Author: Solit
 */
#include <stdint.h>
#include <string.h>
#include <avr/sfr_defs.h>
#include "ha-common.h"
#include "ha-nlink.h"

nlink_t nlink; /* 131B */

#define NODE_FLAG_VALID 1      // for sanity purposes
#define NODE_FLAG_RX    2      //

// TODO: include int nlink - implement as double a buffer concept
uint8_t g_rx_buf[NLINK_COMM_BUF_SIZE];    /*  get rid off it stop process incoming package until this one being processed */

static void isr_ha_nlink_io_set_idle()
{ // LVL-2 +2

    nlink.io.state = NLINK_IO_STATE_IDLE;
    nlink.io.idle_timer = 0;
    NLINK_RX_INT_ENABLE;
    NLINK_IO_TIMER_ENABLE;
}
static void isr_ha_nlink_io_recover()
{ // LVL-2 +2
    
    NLINK_IO_TX_PORT &= ~NLINK_IO_TX_PIN_MASK;
    nlink.io.state = NLINK_IO_STATE_RECOVERING;
    nlink.io.recover_timer = NLINK_IO_RECOVER_TIMER;
    nlink.io.rx_wr = 0;
    NLINK_RX_INT_ENABLE;        // Recovery timer will be restarted on pin fall
    NLINK_IO_TIMER_ENABLE;
}

void ha_nlink_init()
{
    memset((uint8_t*)&nlink, 0, sizeof(nlink_t));

    ha_nlink_gpio_init();

    isr_ha_nlink_io_recover();
}

node_t* ha_nlink_node_register(uint8_t addr, uint8_t type, node_rx_cb_t on_rx_cb, void* on_rx_cb_ctx)
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
    node->on_rx_cb_ctx = on_rx_cb_ctx;
    return node;
}

/*
 * The function returns TRUE (consumed) if receiving messages
 * was fully process and further processing not supposed. This is
 * typically for direct node addressing.
 */
uint8_t nlink_node_on_rx(uint8_t *rx_buf)
{ 
    // LVL2       +8  +2    nlink_node_on_rx
    //   LVL3     +7  +2    ledlight_on_rx
    //     LVL4   +2  +2    ha_node_ledlight_off

    uint8_t consumed = 0;
    uint8_t addr_to = rx_buf[NLINK_HDR_OFF_TO];
    uint8_t addr_from = rx_buf[NLINK_HDR_OFF_FROM];

    consumed = 0;
    for (uint8_t i = 0; i < NLINK_NODES_NUM; i++) {
        node_t *node = &nlink.nodes[i];
        if (node->addr == 0) break;

        // don't receive own messages
        if (node->addr == addr_from) continue;

        if ( node->addr == addr_to ||               // Direct message
             node->type == NODE_TYPE_CTRLCON ||     // CTRLCON listens for all messages
             addr_to == NODE_ADDR_BC ) {            // Broadcast message
            // Address matched
            consumed |= (node->addr == addr_to);

            if (rx_buf[NLINK_HDR_OFF_CMD] == NLINK_CMD_RD_REQ) {
                // RD_REQ doesn't require node involving just send node's TX buffer
                ha_nlink_node_send(node, addr_from, NLINK_CMD_RD_RESP);
            } else {
                node->on_rx_cb(node->on_rx_cb_ctx, rx_buf);
                #if 0
                    ledlight_on_rx();
                    switch_on_rx();
                #endif                
            }
        }
    }

    return consumed;
}
void ha_nlink_node_send(node_t *node, uint8_t addr_to, uint8_t cmd)
{
// LVL - static
    node->tx_buf[NLINK_HDR_OFF_FROM] = node->addr;
    node->tx_buf[NLINK_HDR_OFF_TO  ] = addr_to;
    node->tx_buf[NLINK_HDR_OFF_CMD ] = cmd;

    // Mark as "to be send"
    node->tx_flag = 1;
}

void ha_nlink_check_rx()
{
    // LVL1         +0  +2    ha_nlink_check_rx
    //   LVL2       +8  +2    nlink_node_on_rx
    //     LVL3     +7  +2    ledlight_on_rx
    //       LVL4   +2  +2    ha_node_ledlight_off
    
    if (nlink.io.state != NLINK_IO_STATE_IDLE)
        return;

    // Data can be read after 1 clock in idle state.
    // This is because TX should invalidate RX data after check in
    // order to avoid receiving own messages via HW.
    // Att: Communication between local nodes doesn't involve nlink IO HW!
    if (nlink.io.idle_timer < 1)
        return;

    uint8_t rx_wr = nlink.io.rx_wr;

    // Check is header received
    if ( rx_wr > NLINK_HDR_OFF_LEN) {
        // Check  is full message received
        if (rx_wr == nlink.io.rx_buf[NLINK_HDR_OFF_LEN] + NLINK_HDR_OFF_DATA) {

            // Make local buffer copy and reset write index
            memcpy(g_rx_buf, nlink.io.rx_buf, rx_wr);
            nlink.io.rx_wr = 0;

            nlink_node_on_rx(g_rx_buf);
        } else {
            // If IDLE timeout already expired, but message still not
            // completed, then consider it as an error and do recover
            if (nlink.io.idle_timer == NLINK_IO_IDLE_TIMEOUT) {
                isr_ha_nlink_io_recover();
            }
        }

    }
}
uint8_t ha_nlink_on_tx_default(uint8_t idx, uint8_t *buf)
{
	node_t *node = &nlink.nodes[idx];

//	assert(node->tx_flag != 0)

	node->tx_flag = 0;
	uint8_t tx_buf_len = node->tx_buf[NLINK_HDR_OFF_LEN] + NLINK_HDR_OFF_DATA;
	memcpy(buf, node->tx_buf, tx_buf_len);
	return tx_buf_len;
}

void ha_nlink_check_tx()
{ 
    // LVL          +4  +2    ha_nlink_check_tx
    //   LVL2       +8  +2    nlink_node_on_rx
    //     LVL3     +7  +2    ledlight_on_rx
    //       LVL4   +2  +2    ha_node_ledlight_off

    if (nlink.io.state != NLINK_IO_STATE_IDLE ||
        nlink.io.idle_timer != NLINK_IO_IDLE_TIMEOUT) {
        return;
    }

    // IO Idle timeout expired
    // Check pending transfer (in case of previous one failed)
    if (nlink.io.tx_len) {
        cli();
            nlink.io.tx_rd = 0;
            isr_ha_nlink_io_set_idle(); // Restart previous transfer
        sei();
        return;
    }
    // Get data to transfer from nodes
    for(uint8_t i = 0; i < ARRAY_SIZE(nlink.nodes); i++) {
        node_t *node = &nlink.nodes[i];
		uint8_t tx_buf_len;
		if (node->tx_flag == 0) {
			continue;
		}

		tx_buf_len = ha_nlink_on_tx_default(i, nlink.io.tx_buf);

		// Initiate transfer in next timer interrupt
		if (tx_buf_len) {
            // Check local nodes
            if (nlink_node_on_rx(nlink.io.tx_buf)) {
                return;
            }

            // Initiate transfer
            cli();
			    nlink.io.tx_len = tx_buf_len;
			    nlink.io.tx_rd = 0;
			    isr_ha_nlink_io_set_idle();
            sei();
		}
    }
}

static void isr_nlink_io_rx_on_idle()
{
    if (nlink.io.idle_timer < NLINK_IO_IDLE_TIMEOUT) {
        nlink.io.idle_timer++;
    } else {
        NLINK_IO_TIMER_DISABLE;
    }
}

static void isr_nlink_io_rx_on_receiving() 
{

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
            isr_ha_nlink_io_recover();
        }
        return;
    }

    if (bit_cnt == 9) {
        // Check STOP bit
        if (!bit_in) {
            isr_ha_nlink_io_recover();
        } else {
            if (nlink.io.rx_wr == sizeof(nlink.io.rx_buf)) {
                isr_ha_nlink_io_recover();
            } else {
                nlink.io.rx_buf[nlink.io.rx_wr++] = nlink.io.rx_shift_reg;
                isr_ha_nlink_io_set_idle(); // Wait for next byte start
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
            if (nlink.io.tx_len == 0) {
                break;
            }
            if (nlink.io.tx_rd < nlink.io.tx_len) {
                // Something to send
                nlink.io.tx_shift_reg = nlink.io.tx_buf[nlink.io.tx_rd++];
                // Start bit
                NLINK_IO_TX_PORT |= NLINK_IO_TX_PIN_MASK;
            } else if (nlink.io.tx_rd == nlink.io.tx_len) {
                // Check that transmitted header equals to received
                if ( nlink.io.rx_buf[0] == nlink.io.tx_buf[0] &&
                     nlink.io.rx_buf[1] == nlink.io.tx_buf[1]) {
                    // Transfer is OK
                    nlink.io.tx_rd = 0;
                    nlink.io.tx_len = 0;
                } else {
                    // Transmittion failed.
                    // Use tx_rd > tx_len as retransmit required flag
                    // Retransmittion will be initiated in idle loop
                    // after idle timeout expired
                    nlink.io.tx_rd = 0xFF; // aka (tx_len + 1)
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
                isr_ha_nlink_io_set_idle();
            }
            break;

        case NLINK_IO_STATE_IDLE:
            isr_nlink_io_rx_on_idle();
            break;
        default:
            FATAL_TRAP(__LINE__);
    }
}
/*
 * NLINK IO Timer callback should be called by main logic
 * every 256usec
 */
void isr_nlink_io_on_timer ()
{ 
//   LVL-1 +2 +2
// LVL-2 +2         isr_nlink_io_on_rx_timer -> ha_nlink_io_set_idle

    if (nlink.io.is_rx_timer) {
        NLINK_IO_DBG_PORT |= NLINK_IO_DBG_PIN0_MASK;
        isr_nlink_io_on_rx_timer();
    } else {
        NLINK_IO_DBG_PORT &= ~NLINK_IO_DBG_PIN0_MASK;
        isr_nlink_io_on_tx_timer();
    }
    nlink.io.is_rx_timer = !nlink.io.is_rx_timer;
    
}

void isr_nlink_io_on_start_edge ()
{
    // Triggered on RX falling edge
    if (nlink.io.state == NLINK_IO_STATE_RECOVERING) {
        nlink.io.recover_timer = NLINK_IO_RECOVER_TIMER;
        return;
    }

    NLINK_RX_INT_DISABLE;
    nlink.io.is_rx_timer = 1;      // Next timer interrupt will be RX
    nlink.io.bit_cnt = 0;
    nlink.io.state = NLINK_IO_STATE_ACTIVE;

    NLINK_IO_TIMER_ENABLE;
}
