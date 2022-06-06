/*
* ha_dev_base.h
*
* Created: 3/30/2019 1:14:07 PM
*  Author: solit
*/
#pragma once

#include <avr/io.h>

extern int8_t g_ha_nlink_timer_cnt;
#define NLINK_IO_TIMER_ENABLE  g_ha_nlink_timer_cnt = 0
#define NLINK_IO_TIMER_DISABLE g_ha_nlink_timer_cnt = -1

#define NLINK_RX_INT_ENABLE \
    do { \
        PORTC.PIN5CTRL = PORT_ISC_FALLING_gc | PORT_PULLUPEN_bm;\
        NLINK_IO_RX_INTFLAG = NLINK_IO_RX_PIN_MASK;\
    } while(0)    // Falling edge + Clear interrupt flag if pending

#define NLINK_RX_INT_DISABLE \
    do {\
        PORTC.PIN5CTRL = PORT_ISC_INTDISABLE_gc;\
        NLINK_IO_RX_INTFLAG = NLINK_IO_RX_PIN_MASK;\
    } while(0)

//#define NLINK_IO_TIMER_ENABLE  do {TIFR |= _BV(TOV2); TIMSK |= _BV(TOIE2); TCNT2 = 0;} while(0)
//#define NLINK_IO_TIMER_DISABLE TIMSK &= ~_BV(TOIE2)

// ATTiny817 / NLINK_BASE
#define NLINK_IO_RX_PORT    VPORTC.OUT
#define NLINK_IO_RX_DIR     VPORTC.DIR
#define NLINK_IO_RX_PIN     VPORTC.IN
#define NLINK_IO_RX_INTFLAG VPORTC.INTFLAGS
#define NLINK_IO_RX_PIN_MASK   _BV(5)       // PC5

#define NLINK_IO_TX_PORT    VPORTC.OUT
#define NLINK_IO_TX_DIR     VPORTC.DIR
#define NLINK_IO_TX_PIN_MASK   _BV(4)       // PC4

#define NLINK_IO_DBG_PIN0_MASK _BV(3)       // PC3
#define NLINK_IO_DBG_PIN1_MASK _BV(2)       // PC2 Shared with C14

#define NLINK_IO_DBG_PORT   VPORTC.OUT
#define NLINK_IO_DBG_DIR    VPORTC.DIR
#define NLINK_IO_DBG_PIN    VPORTC.IN

#define IN_SW_0_PORT PORTA
#define IN_SW_1_PORT PORTA
#define IN_SW_2_PORT PORTB
#define IN_SW_3_PORT PORTB
#define IN_SW_4_PORT PORTB
#define IN_SW_5_PORT PORTC
#define IN_SW_6_PORT PORTC

#define IN_SW_0_VPIN VPORTA.IN
#define IN_SW_1_VPIN VPORTA.IN
#define IN_SW_2_VPIN VPORTB.IN
#define IN_SW_3_VPIN VPORTB.IN
#define IN_SW_4_VPIN VPORTB.IN
#define IN_SW_5_VPIN VPORTC.IN
#define IN_SW_6_VPIN VPORTC.IN

#define IN_SW_0_MASK _BV(6) // PINA6 C2
#define IN_SW_1_MASK _BV(7) // PINA7 C4
#define IN_SW_2_MASK _BV(5) // PINB5 C6
#define IN_SW_3_MASK _BV(4) // PINB4 C8
#define IN_SW_4_MASK _BV(3) // PINB3 C10
#define IN_SW_5_MASK _BV(0) // PINC0 C12
#define IN_SW_6_MASK _BV(2) // PINC2 C14

#define IN_SW_0_PIN (6) // PINA6 C2
#define IN_SW_1_PIN (7) // PINA7 C4
#define IN_SW_2_PIN (5) // PINB5 C6
#define IN_SW_3_PIN (4) // PINB4 C8
#define IN_SW_4_PIN (3) // PINB3 C10
#define IN_SW_5_PIN (0) // PINC0 C12
#define IN_SW_6_PIN (2) // PINC2 C14

#define IN_SW_0_PORT_CTRL  *(&(IN_SW_0_PORT.PIN0CTRL) + IN_SW_0_PIN)
#define IN_SW_1_PORT_CTRL  *(&(IN_SW_1_PORT.PIN0CTRL) + IN_SW_1_PIN)
#define IN_SW_2_PORT_CTRL  *(&(IN_SW_2_PORT.PIN0CTRL) + IN_SW_2_PIN)
#define IN_SW_3_PORT_CTRL  *(&(IN_SW_3_PORT.PIN0CTRL) + IN_SW_3_PIN)
#define IN_SW_4_PORT_CTRL  *(&(IN_SW_4_PORT.PIN0CTRL) + IN_SW_4_PIN)
#define IN_SW_5_PORT_CTRL  *(&(IN_SW_5_PORT.PIN0CTRL) + IN_SW_5_PIN)
#define IN_SW_6_PORT_CTRL  *(&(IN_SW_6_PORT.PIN0CTRL) + IN_SW_6_PIN)

// Lxxx => WO0..WO2 => C11, C9, C7
// Hxxx => WO3..WO5 =>  C5, C3, C1

#define OUT_0_PORT PORTA
#define OUT_0_PIN (5)                   // PINA5 C1
#define OUT_0_MASK _BV(OUT_0_PIN)   

#define OUT_1_PORT PORTA
#define OUT_1_PIN (4)                   // PINA4 C3
#define OUT_1_MASK _BV(OUT_1_PIN)

#define OUT_2_PORT PORTA
#define OUT_2_PIN (3)                   // PINA3 C5
#define OUT_2_MASK _BV(OUT_2_PIN)

#define OUT_3_PORT PORTB
#define OUT_3_PIN (2)                   // PINB2 C7
#define OUT_3_MASK _BV(OUT_3_PIN)

#define OUT_4_PORT PORTB
#define OUT_4_PIN (1)                   // PINB1 C9
#define OUT_4_MASK _BV(OUT_4_PIN)

#define OUT_5_PORT PORTB
#define OUT_5_PIN (0)                   // PINB0 C11
#define OUT_5_MASK _BV(OUT_5_PIN)

#define OUT_6_PORT PORTC
#define OUT_6_PIN (1)                   // PINC1 C13
#define OUT_6_MASK _BV(OUT_6_PIN)

#define INTENSITIES_NUM 19

extern void ha_dev_base_init();
extern void ha_dev_base_set_steady(uint8_t led_num, uint8_t val);
extern void ha_dev_base_set_slow_pwm(uint8_t led_num, uint8_t pwm_val_idx);
extern void ha_dev_base_set_fast_pwm(uint8_t led_num, uint8_t pwm_val_idx);
extern uint8_t ha_dev_base_get_in_pins();

/*
 * Function that need to be provided by the app
 */
extern void isr_ha_app_on_timer();
