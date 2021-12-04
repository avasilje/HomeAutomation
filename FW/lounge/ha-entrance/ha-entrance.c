/*
 * TODO:
 *   1. Make PHTS as part of node METEO
 *   2. Add PHTS sensor error handling
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ha-common.h"
#include "ha-uart.h"
#include "ha-nlink.h"
#include "ha-i2c.h"
#include "ha-node-switch.h"
#include "ha-node-ctrlcon.h"
#include "ha-node-phts.h"
#include "ha-entrance.h"

HW_INFO gt_hw_info __attribute__ ((section (".act_const"))) = {0, 2, "AV HA Entrance"};
PF_PVOID gpf_action_func;

extern T_ACTION gta_action_table[];

uint16_t gus_trap_line;

void action_default();

void FATAL_TRAP (uint16_t us_line_num) {
    gus_trap_line = us_line_num;
    while(1);
}

STATIC_ASSERT( (HA_UART_RX_BUFF_SIZE - UART_CC_HDR_SIZE) > NLINK_COMM_BUF_SIZE, UART_RX_BUFFER_TO_SMALL);
STATIC_ASSERT( (HA_UART_TX_BUFF_SIZE - UART_CC_HDR_SIZE) > NLINK_COMM_BUF_SIZE, UART_TX_BUFFER_TO_SMALL);

// ---------------------------------
// --- LEDS specific
// ---------------------------------
#define LED_PORT  PORTB
#define LED_DIR   DDRB
#define LED_PIN   PINB

#define LED_PIN2   PINB2
#define LED_PIN3   PINB3

#define LED_MASK_ALL ((1 << LED_PIN2) |\
                      (1 << LED_PIN3))

#define DISABLE_TIMER0    TIMSK &= ~(1<<TOIE0)            // Disable Overflow Interrupt
#define ENABLE_TIMER0     TIMSK |= (1<<TOIE0)   // Enable Overflow Interrupt
#define CNT_RELOAD       (0xFF - 126)         //

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

static void init_ha_nlink_timer()
{   // Timer is always running
    TCCR2 &= ~(_BV(CS20) | _BV(CS21) | _BV(CS22));
    TCCR2 |= (2 << CS20);    // prescaler 2 => 1/8

    TIMSK |= _BV(TOIE2);
    TCNT2 = 0;               // Free running
    TIFR |= _BV(TOV2);
}

static void init_gpio()
{
    // Set led to pull down
    LED_DIR  |= LED_MASK_ALL;
    LED_PORT &= ~LED_MASK_ALL;
}

void command_lookup(uint8_t uc_msg_cmd)
{
    T_ACTION    *pt_action;
    PF_PVOID    pf_func;

    uint8_t uc_act_cmd;

    // --------------------------------------------------
    // --- Find received command in actions' table
    // --------------------------------------------------
    pt_action = gta_action_table;

    // Check functions table signature
    pf_func = (PF_PVOID)pgm_read_word_near(&pt_action->pf_func);
    if (pf_func != (PF_PVOID)0xFEED)
        FATAL_TRAP(__LINE__);

    // Find action in table
    do {
        pt_action ++;
        uc_act_cmd = pgm_read_byte_near(&pt_action->uc_cmd);

        // if unknown command received (end of table reached) just
		// readout from UART amount of bytes specified in header
        if (uc_act_cmd == 0xFF) {
            gpf_action_func = action_default;
			break;
		}

        if (uc_msg_cmd == uc_act_cmd){
            // get functor from table
            gpf_action_func = (PF_PVOID)pgm_read_word_near(&pt_action->pf_func);
            break;
        }
    } while(1); // End of action table scan

    gpf_action_func();

    return;
}

void init_timer()
{
    // timer - Timer0. Incrementing counting till UINT8_MAX
    TCCR0 = CNT_TCCRxB;
    TCNT0 = CNT_RELOAD;
    ENABLE_TIMER0;
    guc_timer_cnt = 0;
}

int main(void)
{
    OSCCAL = 0xAC;  // TODO: need to be stored in FLASH upon programming
                    // Actual value written to entrance dev board 0xAA! Clarify

    init_gpio();

    NLINK_IO_DBG_DIR |= (NLINK_IO_DBG_PIN0_MASK | NLINK_IO_DBG_PIN1_MASK);
    NLINK_IO_DBG_PORT &= ~(NLINK_IO_DBG_PIN0_MASK | NLINK_IO_DBG_PIN1_MASK);

    init_timer();

    init_ha_nlink_timer();
    // Wait a little just in case
    for(uint8_t uc_i = 0; uc_i < 255U; uc_i++) {

        __asm__ __volatile__ ("    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"
                                ::);
    }

    ha_uart_init();

    // Set INT0 trigger to Falling Edge, Clear interrupt flag
    MCUCR &= ~(_BV(ISC00) | _BV(ISC01));
    MCUCR |= (2 << ISC00);
    ha_nlink_init();

    ha_i2c_init();

    ha_node_phts_init();
    ha_node_switch_init();
    ha_node_ctrlcon_init(); // Ctrlcon node must be initialized last because collects info about all other nodes

    sei();

    while(1) {
        ha_i2c_on_idle();
        ha_uart_check_tx();
        ha_uart_check_rx();
        ha_nlink_check_rx();
        ha_nlink_check_tx();
        ha_node_phts_on_idle();
    }
    cli();

}

ISR(TIMER2_OVF_vect)
{
    // 8*256 clocks @ 8MHz = 8*64us = 256us = 1/2 nlink clock period
    //TCNT2 = 0;
    isr_nlink_io_on_timer();
}

ISR(INT0_vect)
{
    isr_nlink_io_on_start_edge();
}

ISR(INT1_vect)
{
    ha_i2c_isr_on_scl_edge();
}

static void isr_sw_scl_edge_detection()
{
    static uint8_t prev = I2C_SCL_MSK;
    uint8_t curr = I2C_SCL_PIN & I2C_SCL_MSK;

    if (curr && !prev) {
        ha_i2c_isr_on_scl_edge();
    }
    prev = curr;
}

ISR(TIMER0_OVF_vect) {

#define TIMER_CNT_PERIOD 80   // 100Hz    10ms. Precision 125usec
    // --------------------------------------------------
    // --- Reload Timer
    // --------------------------------------------------
    TCNT0 = CNT_RELOAD;

    ha_i2c_isr_on_timer();

    isr_sw_scl_edge_detection();

    // Update global PWM counter
    guc_timer_cnt ++;
    if (guc_timer_cnt == TIMER_CNT_PERIOD) {
        guc_timer_cnt = 0;
    }

    // --------------------------------------------------
    // --- Check switches
    // --- results in globals: guc_sw_event_hold, guc_sw_event_push
    // --------------------------------------------------
    if (guc_timer_cnt == 1){
        // Every 10ms
        ha_node_switch_on_timer();

        ha_uart_on_timer();

        ha_node_phts_on_timer();
    }
}

void action_default()
{
	// Command format
	// 0x41 0x86    0x??       0xLL       ...
	// MARK         Any CMD    Length     xxx

	// Receive full message
	uint8_t len = UART_CC_HDR_SIZE + ha_uart.rx_buff[UART_CC_HDR_LEN];
	while(ha_uart.rx_wr_idx < len);
 //!!!!!!!!!!! debug this with -O2 !!!!!!!!!!
 //!!! debug to often resync 1. reduce baudrate; try -02 !!!
}

void action_signature()
{
    // uint8_t uc_i;

    // Command format
    // 0x41 0x86 0x21  0xLL    0xAA
    // MARK      CMD   Length  Address

    // Response format
    // 0x41 0x86 0x11    0xLL    0xMM    0xMM    0xCC
    // MARK      Length  Major   Minor   String name

    // Check is message length available, wait if not yet
    while(ha_uart.rx_wr_idx < 5);
/*
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
    if (ha_uart.rx_wr_idx != 2)
        FATAL_TRAP(__LINE__);

    guc_out_msg_wr_idx = SIGN_LEN + 6;
*/
    // End of action_signature
    return;
}

void action_ctrlcon_get()
{
    // Command format
    // 0x41 0x86 0x21  0xLL    0xAA
    // MARK      CMD   Length  Address
#define CC_ACTION_GET_OFF_ADDR	0
#define CC_ACTION_GET_SIZE	(CC_ACTION_GET_OFF_ADDR + 1)

    // Response format (NLINK_HDR_OFF_xxx)
    // 0x41 0x86 0x21  0xLL    | 0xAA        0xAA     0xCC   0xTT  0xLL      0xDD         |
    // MARK      CMD   Length  | Addr_From   Addr_To  CMD    Type  Data Len  node info    |

    // Sanity - check message length
    if (ha_uart.rx_buff[UART_CC_HDR_LEN] != CC_ACTION_GET_SIZE) {
        ha_uart_resync();
    }

    // Check is message address already available, wait if not yet
    while(ha_uart.rx_wr_idx < UART_CC_HDR_SIZE + CC_ACTION_GET_SIZE);

    // Get node address
    uint8_t req_addr = ha_uart.rx_buff[UART_CC_HDR_SIZE + CC_ACTION_GET_OFF_ADDR];

    // Set RX flag in peer to initiate transfer node info to UART
    // The transfer will be initiated later in the idle loop
    ha_node_ctrlcon_peer_set_rx(req_addr);
}

void action_ctrlcon_set()
{
    // Command format
    // 0x41 0x86 0x22  0xLL    |
    // MARK      CMD   Length  |

	// Receive full message
	action_default();

	ha_node_ctrlcon_on_rx_uart(&ha_uart.rx_buff[UART_CC_HDR_DATA], ha_uart.rx_buff[UART_CC_HDR_LEN]);
}

void action_ctrlcon_disc()
{
    // Command format
    // 0x41 0x86 0x23  0xLL    |
    // MARK      CMD   Length  |

	// Receive full message
	action_default();

    ha_node_ctrlcon_discover();
}

T_ACTION gta_action_table[] __attribute__ ((section (".act_const"))) = {
    { "mark", 0x00, (void*)0xFEED        },    // Table signature
    { "sign", 0x11, action_signature     },
    { "cc_g", 0x21, action_ctrlcon_get   },
    { "cc_s", 0x22, action_ctrlcon_set   },
    { "cc_d", 0x23, action_ctrlcon_disc  },
    { ""    , 0xFF, NULL                 }     // End of table
};


