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

// ATTiny4313
#define NLINK_IO_RX_PORT  PORTB
#define NLINK_IO_RX_DIR   DDRB
#define NLINK_IO_RX_PIN   PINB
#define NLINK_IO_RX_PIN_MASK   _BV(PINB0)   // PCINT0

#define NLINK_IO_TX_PORT  PORTB
#define NLINK_IO_TX_DIR   DDRB
#define NLINK_IO_TX_PIN_MASK   _BV(PINB1)

#define NLINK_IO_DBG_PIN0_MASK _BV(PIND0)
#define NLINK_IO_DBG_PIN1_MASK _BV(PIND1)
#define NLINK_IO_DBG_PORT PORTD
#define NLINK_IO_DBG_PIN  PIND
#define NLINK_IO_DBG_DIR  DDRD

// ATTiny4313
#define ENABLE_PCINT  do {GIFR |= (1 << PCIF0); GIMSK |= (1 << PCIE0); } while(0)
#define DISABLE_PCINT  GIMSK &= ~(1 << PCIE0)

#define NLINK_RX_INT_ENABLE ENABLE_PCINT
#define NLINK_RX_INT_DISABLE DISABLE_PCINT

/********************************************/
#define NLINK_IO_DBG_PIN0_MASK _BV(PIND0)
#define NLINK_IO_DBG_PIN1_MASK _BV(PIND1)
#define NLINK_IO_DBG_PORT PORTD
#define NLINK_IO_DBG_PIN  PIND
#define NLINK_IO_DBG_DIR  DDRD

#define IN_SW_PORT PORTB
#define IN_SW_PIN  PINB
#define IN_SW_DIR  DDRB

#define IN_SW_0_MASK _BV(7) // PINB7 C0
#define IN_SW_1_MASK _BV(6) // PINB6 C1
//#define IN_SW_2_MASK _BV(0) // PINB0 C2
//#define IN_SW_3_MASK _BV(1) // PINB1 C3

#define IN_SW_0     PINB7
#define IN_SW_1     PINB6
//#define IN_SW_2     PINB0
//#define IN_SW_3     PINB1

#define IN_SW_MASK \
            ((1 << IN_SW_0)|\
             (1 << IN_SW_1))

/************************************************/
#define OUT_LED_012_PORT    PORTB
#define OUT_LED_012_PIN     PINB
#define OUT_LED_012_DIR     DDRB

#define OUT_LED_3_PORT      PORTD
#define OUT_LED_3_PIN       PIND
#define OUT_LED_3_DIR       DDRD

#define OUT_LED_0           PINB2
#define OUT_LED_1           PINB3
#define OUT_LED_2           PINB4
#define OUT_LED_3           PIND5

#define OUT_LED_0_MASK      _BV(OUT_LED_0)
#define OUT_LED_1_MASK      _BV(OUT_LED_1)
#define OUT_LED_2_MASK      _BV(OUT_LED_2)
#define OUT_LED_3_MASK      _BV(OUT_LED_3)

#define OUT_LED_012_MASK \
            ((OUT_LED_0_MASK) |\
             (OUT_LED_1_MASK) |\
             (OUT_LED_2_MASK))

#define INTENSITIES_NUM 19

extern void ha_dev_leds_init();
extern void ha_dev_leds_set_steady(uint8_t led_num, uint8_t val);
extern void ha_dev_leds_set_fast_pwm(uint8_t led_num, uint8_t pwm_val_idx);
extern uint8_t ha_dev_leds_get_in_pins();

/*
 * Function that need to be provided by the app
 */
extern void isr_ha_app_on_timer();
