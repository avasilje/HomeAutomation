/*
 * TODO:
 * 1. get rid of node allocation on new ctrlcon peers
 */

/*
 * node-entrance.c
 *
 * Created: 08/04/2018
 *  Author: Solit
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <string.h>
#include "ha-common.h"
#include "ha-nlink.h"
#include "ha-node-switch.h"
#include "ha-node-ctrlcon.h"
#include "ha-entrance.h"

HW_INFO gt_hw_info __attribute__ ((section (".act_const"))) = {0, 2, "AV HA Entrance"};
PF_PVOID gpf_action_func;

extern T_ACTION gta_action_table[];

uint16_t gus_trap_line;

void FATAL_TRAP (uint16_t us_line_num) { 
    gus_trap_line = us_line_num;
    while(1);
}


#define MSG_BUFF_SIZE 32
volatile uint8_t guc_in_msg_idx;
uint8_t guca_in_msg_buff[MSG_BUFF_SIZE];

volatile uint8_t guc_out_msg_wr_idx; 
volatile uint8_t guc_out_msg_rd_idx;
uint8_t guca_out_msg_buff[MSG_BUFF_SIZE];   // ??  not in use. see ha_uart

// ---------------------------------
// --- LEDS specific                                
// ---------------------------------
#define LED_PORT  PORTB
#define LED_DIR   DDRB
#define LED_PIN   PINB

#define LED_PIN0   PINB0
#define LED_PIN1   PINB1
#define LED_PIN2   PINB2
#define LED_PIN3   PINB3

#define LED_MASK_ALL ((1 << LED_PIN0) |\
                      (1 << LED_PIN1) |\
                      (1 << LED_PIN2) |\
                      (1 << LED_PIN3))

#define DISABLE_TIMER    TIMSK = 0            // Disable Overflow Interrupt
#define ENABLE_TIMER     TIMSK = (1<<TOIE0)   // Enable Overflow Interrupt
#define CNT_RELOAD       (0xFF - 125)         // 

#define CNT_TCCRxB       2    // Prescaler value (2->1/8)

#ifndef DBG_EN
#define DBG_EN 0
#endif

#if DBG_EN
#define DBG_VAR_NUM 24
uint16_t dbg_p[DBG_VAR_NUM];
uint8_t dbg_idx = 0;
#endif

uint8_t guc_timer_cnt;

typedef struct ha_uart_s {
    uint8_t tx_buf[NLINK_COMM_BUF_SIZE];
    uint8_t tx_rd_idx;
    uint8_t tx_len;
} ha_uart_t;

ha_uart_t ha_uart;

static void init_uart()
{
    // Init UART to 0.5Mbps 8N1

    // Set baud rate. UBRR = Fosc/8/BAUD - 1
    // Fosc = 8MHz, UBRR = 8*10^6/16/250000 - 1 = 1
    UBRRH = 0;
    UBRRL = 1;

    // Set frame format: 8data, 1stop bit, no parity */
    UCSRC = 
        _BV(URSEL) |    // Enable access to UCSRC. Otherwise UBRRH will be overwritten
        _BV(UCSZ0) |    // 8 data bits
        _BV(UCSZ1);     // 8 data bits

    // Enable receiver and transmitter
    UCSRB =
        _BV(RXCIE)|     // RX Interrupt enable
        _BV(RXEN) |     // Receiver enabled
        _BV(TXEN);      // Transmitter enabled

    ha_uart.tx_rd_idx = 0;
    ha_uart.tx_len = 0;
}

void ha_uart_enable_tx() {
    UCSRB |= _BV(UDRIE);  // TX Interrupt enable
        // cc.peers[idx].flags |= PEER_FLAG_RX;
}
 

static void init_gpio(){

    // Set led to pull down
    LED_DIR  |= LED_MASK_ALL;
    LED_PORT &= ~LED_MASK_ALL;
}

static void command_lookup()
{
    T_ACTION    *pt_action;
    PF_PVOID    pf_func;

    uint8_t uc_act_cmd;
    uint8_t uc_msg_cmd;
    
    // check is command valid
    // MARK = "AV", read pointer must be set on buffer start
    if ( guca_in_msg_buff[0] != 0x41)
        FATAL_TRAP(__LINE__);

    if ( guca_in_msg_buff[1] != 0x86)
        FATAL_TRAP(__LINE__);

    uc_msg_cmd = guca_in_msg_buff[2];

    // --------------------------------------------------
    // --- Find received command in actions' table
    // --------------------------------------------------
    pt_action = gta_action_table;

    // Check functions table signature
    pf_func = (PF_PVOID)pgm_read_word_near(&pt_action->pf_func);
    if (pf_func != (PF_PVOID)0xFEED)
        FATAL_TRAP(__LINE__);

    // Find action in table
    do 
    {
        pt_action ++;
        uc_act_cmd = pgm_read_byte_near(&pt_action->uc_cmd);

        // TRAP if unknown command received (end of table reached)
        if (uc_act_cmd == 0xFF) 
            FATAL_TRAP(__LINE__);

        if (uc_msg_cmd == uc_act_cmd){
            // get functor from table
            gpf_action_func = (PF_PVOID)pgm_read_word_near(&pt_action->pf_func);
            break;
        }
    } while(1); // End of action table scan

    gpf_action_func();

    // Clear RX buffer index in order to receive next message
    guc_in_msg_idx = 0;

    return;
}

void init_timer() {
    // timer - Timer0. Incrementing counting till UINT8_MAX
    TCCR0 = CNT_TCCRxB;
    TCNT0 = CNT_RELOAD;
    ENABLE_TIMER;
    guc_timer_cnt = 0;
}

int main(void)
{
    init_gpio();
    
    init_timer();

    // Wait a little just in case
    for(uint8_t uc_i = 0; uc_i < 255U; uc_i++){

        __asm__ __volatile__ ("    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"
                                ::);
    }

    init_uart();
    ha_nlink_init();

    ha_node_switch_init();
    ha_node_ctrlcon_init(); // Ctrlcon node must be initialized last because collects info about all other nodes
    
    sei();

    while(1) 
    {
        // Get number of bytes received (4 bytes at least)
        if (guc_in_msg_idx >= 4)
        {
            // Init transmitter's buffer
            guc_out_msg_wr_idx = 0;
            guc_out_msg_rd_idx = 0;

            // Find and execute command from the ACTIONS TABLE
            command_lookup();
        }
    } 
    cli();

}

ISR(TIMER0_OVF_vect) {

#define TIMER_CNT_PERIOD 80   // 100Hz    10ms. Precision 125usec
    // --------------------------------------------------
    // --- Reload Timer
    // --------------------------------------------------
    TCNT0 = CNT_RELOAD;

    // Update global PWM counter
    guc_timer_cnt ++;
    if (guc_timer_cnt == TIMER_CNT_PERIOD)
    {
        guc_timer_cnt = 0;
    }

    // --------------------------------------------------
    // --- Check switches
    // --- results in globals: guc_sw_event_hold, guc_sw_event_push
    // --------------------------------------------------
    if (guc_timer_cnt == 1)
    {   // Every 10ms
        ha_node_switch_on_timer();
    }
}


ISR(USART_UDRE_vect) {

    // Unload TX buffer if not empty
    if (ha_uart.tx_rd_idx != ha_uart.tx_len) {
        UDR = ha_uart.tx_buf[ha_uart.tx_rd_idx];
        UCSRA |= (1 << TXC); // Clear transmit complete flag
        ha_uart.tx_rd_idx ++;
        return;
    }

    // Nothing to sent - check ctrlcon
    ha_uart.tx_len = ha_node_ctrlcon_to_sent(ha_uart.tx_buf);
    if (ha_uart.tx_len) {
        // new data received - initialize TX buffer and sent 1st byte
        UDR = ha_uart.tx_buf[0];
        UCSRA |= (1 << TXC); // Clear transmit complete flag
        ha_uart.tx_rd_idx = 1;
        return;
    }

    // No more data - disable interrupt
    UCSRB &= ~_BV(UDRIE);  // TX Interrupt enable
}

ISR(USART_RXC_vect) {

    uint8_t uc_status;
    uint8_t uc_data;

    // get RX status, trap if something wrong
    uc_status = UCSRA;
    uc_data = UDR;

    // Check Frame Error (FE0) and DATA overflow (DOR0)
    if ( uc_status & (_BV(FE) | _BV(DOR)) )
        INT_FATAL_TRAP(__LINE__);

    if ( guc_in_msg_idx == MSG_BUFF_SIZE )
        INT_FATAL_TRAP(__LINE__);

    guca_in_msg_buff[guc_in_msg_idx++] = uc_data;
  
}

void action_signature()
{
    uint8_t uc_i;

    // Command format
    // 0x41 0x86 0x21  0xLL    0xAA      
    // MARK      CMD   Length  Address   

    // Response format
    // 0x41 0x86 0x11    0xLL    0xMM    0xMM    0xCC
    // MARK      Length  Major   Minor   String name

    // Check is message length available, wait if not yet
    while(guc_in_msg_idx < 5);

    // Write header & length
    guca_out_msg_buff[0] = 0x41;
    guca_out_msg_buff[1] = 0x86;

    guca_out_msg_buff[2] = 0x11;
    guca_out_msg_buff[3] = SIGN_LEN + 2;

    // Write version number
    guca_out_msg_buff[4] = gt_hw_info.uc_ver_maj;
    guca_out_msg_buff[5] = gt_hw_info.uc_ver_min;

    // Write signature
    for(uc_i = 0; uc_i < SIGN_LEN; uc_i++)
    {
        guca_out_msg_buff[uc_i+6] = gt_hw_info.ca_signature[uc_i];
    }

    // Check no more input data received
    if (guc_in_msg_idx != 2)
        FATAL_TRAP(__LINE__);

    guc_out_msg_wr_idx = SIGN_LEN + 6;

    // End of action_signature
    return;
}


void node_get_info(uint8_t addr) {

     // for all registered nodes
     {
        
        // If node addr == addr || addr == BC
            {
                // mark node as info requested
            }
     }
}

void action_node_info() 
{
    // Command format
    // 0x41 0x86 0x21  0xLL    0xAA      
    // MARK      CMD   Length  Address   
    
    // Response format
    // 0x41 0x86 0x21  0xLL    | 0xAA      0xTT   0xDD         | 0xAA      0xTT   0xDD      |
    // MARK      CMD   Length  | Address   Type   node info    | Address   Type   node info |  

    // Check is message address is available, wait if not yet
    while(guc_in_msg_idx < 6);

    // Get node address
    uint8_t req_addr = guca_in_msg_buff[4];

    node_get_info(req_addr);
#if 0

    // If our own, then prepare response
    // Retransmit if addr == BC
    if (addr == NODE_ADDR_BC || node_is_own_addr(addr)) {
        
        if (addr == NODE_ADDR_BC) {
            // Copy to NODE_IF
        }
        // Respond with node 
        uint8_t wr_idx = 4
        
        while( node_responde(req_addr, &guca_out_msg_buff[]) ;){

        }
            
    }
    
    uint8_t data_len = guca_out_msg_buff[uc_i+2] = uc_feedback;
    guc_out_msg_wr_idx = uc_i + 2;
#endif
}

T_ACTION gta_action_table[] __attribute__ ((section (".act_const"))) = {
    { "mark", 0x00, (void*)0xFEED        },    // Table signature
    { "sign", 0x11, action_signature     },    
    { "ninf", 0x21, action_node_info     },    
    { ""    , 0xFF, NULL                 }     // End of table
};


