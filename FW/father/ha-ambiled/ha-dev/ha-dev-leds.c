// TODO: !!!!!!!!!!!!!!! Code review !!!!!!!!!!!!!!!
/*
* ha_dev_leds.c
*
* Created: 11/12/2021
*  Author: solit
*/
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/io.h>
#include "ha-common.h"
#include "ha-nlink.h"
#include "ha-dev-leds.h"

int8_t g_ha_nlink_timer_cnt = -1;

#define LEDS_NUM 4

// Note: Values are non linear so need to be calibrated for precise control
#define PWM_PULSE(duty_cycle)  ((duty_cycle * 256 / 100) - 1)
const uint8_t guca_pwm_intensity_table[INTENSITIES_NUM] EEMEM =
{
    0,               // 0   (OFF)
    PWM_PULSE(  3),  // 1
    PWM_PULSE(  6),  // 2
    PWM_PULSE( 10),  // 3
    PWM_PULSE( 14),  // 4
    PWM_PULSE( 18),  // 5
    PWM_PULSE( 23),  // 6
    PWM_PULSE( 28),  // 7
    PWM_PULSE( 33),  // 8
    PWM_PULSE( 38),  // 9
    PWM_PULSE( 44),  // 10
    PWM_PULSE( 50),  // 11
    PWM_PULSE( 56),  // 12
    PWM_PULSE( 63),  // 13
    PWM_PULSE( 70),  // 14
    PWM_PULSE( 75),  // 15
    PWM_PULSE( 85),  // 16
    PWM_PULSE( 93),  // 17
    0xFF             // 18 100% (ON)
};

void in_gpio_init() 
{
    // Pull-up input
    IN_SW_DIR &= ~IN_SW_MASK;
    IN_SW_PORT |= IN_SW_MASK;
}

void out_gpio_init()
{
    // Out switches - output 0
    OUT_LED_012_DIR |= OUT_LED_012_MASK;
    OUT_LED_012_PORT &= ~OUT_LED_012_MASK;

    OUT_LED_3_DIR |= OUT_LED_3_MASK;
    OUT_LED_3_PORT &= ~OUT_LED_3_MASK;
}

uint8_t ha_dev_leds_get_in_pins() {

    uint8_t val = 0;
	uint8_t pins = IN_SW_PIN;
    if (pins & IN_SW_0_MASK) val |= _BV(0);
    if (pins & IN_SW_1_MASK) val |= _BV(1);
//    if (pins & IN_SW_2_MASK) val |= _BV(2);
//    if (pins & IN_SW_3_MASK) val |= _BV(3);

    return val;
}

void fast_pwm_init() 
{
    // Set timer 0 & 1 to 8-bit Fast PWM, no prescaler
    TCCR0A = (1 << WGM00) | (1 << WGM01);
    TCCR0B = (1 << CS00);

    TCCR1A = (1 << WGM10);
    TCCR1B = (1 << WGM12) | (1 << CS10);
}

static void timer_init()
{
    // Enable Timer0 overflow interrupt
    TIMSK = (1<<TOIE0);
}


void ha_nlink_gpio_init()
{
    NLINK_IO_RX_PORT |= NLINK_IO_RX_PIN_MASK;
    NLINK_IO_RX_DIR  &= ~NLINK_IO_RX_PIN_MASK;

    NLINK_IO_TX_PORT &= ~NLINK_IO_TX_PIN_MASK;
    NLINK_IO_TX_DIR  |= NLINK_IO_TX_PIN_MASK;   // output

    PCMSK |= NLINK_IO_RX_PIN_MASK;  // Enable Pin Change interrupt

    NLINK_IO_DBG_PORT &= ~(NLINK_IO_DBG_PIN0_MASK | NLINK_IO_DBG_PIN1_MASK);
    NLINK_IO_DBG_DIR |= (NLINK_IO_DBG_PIN0_MASK | NLINK_IO_DBG_PIN1_MASK);
}

void ha_dev_leds_init()
{
    timer_init();
    in_gpio_init();
    out_gpio_init();
    fast_pwm_init();
}

void ha_dev_leds_set_steady (uint8_t mask, uint8_t val)
{// LVL - static
    // Val 0 ==> On ==> output 1
    // Val 1 ==> Off ==> output 0

    if (mask & 1) {
        // OUT_LED0_0 ==> OC0A (PINB2)
        PORTB = val ? (PORTB | OUT_LED_0_MASK) : (PORTB & ~OUT_LED_0_MASK);
        TCCR0A &= ~((1 << COM0A1) | (1 << COM0A0));
    }
    else if (mask & 2) {
        // OUT_LED0_1 ==> OC1A (PINB3)
        PORTB = val ? (PORTB | OUT_LED_1_MASK) : (PORTB & ~OUT_LED_1_MASK);
        TCCR1A &= ~((1 << COM1A1) | (1 << COM1A0));
    }
    else if (mask & 4) {
        // OUT_SW0_2 ==> OC1B (PINB4)
        PORTB = val ? (PORTB | OUT_LED_2_MASK) : (PORTB & ~OUT_LED_2_MASK);
        TCCR1A &= ~((1 << COM1B1) | (1 << COM1B0));
    }
    else if (mask & 8) {
        // OUT_SW1_3 ==> OC0B (PIND5)
        PORTD = val ? (PORTD | OUT_LED_3_MASK) : (PORTD & ~OUT_LED_3_MASK);
        TCCR0A &= ~((1 << COM0B1) | (1 << COM0B0));
    }
}

void ha_dev_leds_set_fast_pwm (uint8_t mask, uint8_t pwm_val_idx)
{
    //LVL    +2 +2
    //  LVL     +2 eeprom
    
    uint8_t i;
    uint8_t pwm_val = eeprom_read_byte(guca_pwm_intensity_table + pwm_val_idx);
    uint8_t volatile *pwm_reg_ptr = NULL;

    // Select PWM register
    for (i = 0; i < LEDS_NUM; i++) {
        if (0 == (mask & _BV(i))) continue;

        switch(mask & _BV(i)) {
            case 1:
                TCCR0A |= (1 << COM0A1);   // Clear OC0A on Compare Match
                pwm_reg_ptr = &OCR0A;
                break;
            case 2:
                TCCR1A |= (1 << COM1A1);
                pwm_reg_ptr = &OCR1AL;
                break;
            case 4:
                TCCR1A |= (1 << COM1B1);
                pwm_reg_ptr = &OCR1BL;
                break;
            case 8:
                TCCR0A |= (1 << COM0B1);
                pwm_reg_ptr = &OCR0B;
                break;
        }

        // Avoid too small changes in order to avoid flickering
        if (pwm_reg_ptr && abs(pwm_val - *pwm_reg_ptr) > 2) {              /* TODO: review carefully! */
//        if (pwm_reg_ptr && abs(pwm_val - *pwm_reg_ptr) > 2) {
            *pwm_reg_ptr = pwm_val;
        }
    }
}

ISR(PCINT0_vect) 
{ // LVL0 +15

    if ((NLINK_IO_RX_PIN & NLINK_IO_RX_PIN_MASK) == 0) {
        // Call NLINK start callback on RX pin falling edge
        isr_nlink_io_on_start_edge();
    }
}

// Interrupt triggered every 256 timer clocks and count periods
ISR(TIMER0_OVF_vect) 
{ 
//     LVL0   +15 +2
//   LVL-1    +2  +2    isr_nlink_io_on_timer
// LVL-2      +0  +2    isr_nlink_io_on_rx_timer -> ha_nlink_io_set_idle
// ----------------------
//                23
    
    // 256 clocks @ 20 MHz ==> 12.8usec

    // Call NLINK timer callback every 256usec
    if (g_ha_nlink_timer_cnt >= 0) {
        g_ha_nlink_timer_cnt++;
        if (g_ha_nlink_timer_cnt == 20) {
            g_ha_nlink_timer_cnt = 0;
            isr_nlink_io_on_timer();
        }
    }
    
    isr_ha_app_on_timer();    
}
