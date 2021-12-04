#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <stdlib.h>
#include <string.h>
#include "ha-common.h"
#include "ha-ledlight.h"
#include "ha-ledlight_const.h"
#include "ha-nlink.h"
#include "ha-node-switch.h"
#include "ha-node-ledlight.h"

/*
* ha-mainlight.c
*
* Created: 17/03/2019 9:49:58 AM
*  Author: Solit
*/
uint16_t g_led_intensity_cnt = 0;
uint8_t guc_led_intenisity_timer = 0;

uint16_t g_switches_cnt = 0;
uint8_t guc_switches_timer = 0;

int8_t g_ha_nlink_timer_cnt = -1;

// ------- EEPROM ---------------
uint8_t euca_signature[2] __attribute__ ((section (".config_param")));
uint8_t euca_standby_intensity[LEDS_NUM] __attribute__ ((section (".config_param")));
uint8_t euca_on_intensity[LEDS_NUM] __attribute__ ((section (".config_param")));

uint8_t euc_leds_disabled_idx __attribute__ ((section (".config_param")));

uint8_t  guc_sw_event_mask;     //
uint8_t  guc_curr_led_mode;     //

#define EEPROM_SAVE_FLAG_DISABLED  1
#define EEPROM_SAVE_FLAG_INTENSITY 2
uint8_t  guc_eeprom_save_flags;

node_t *g_ll_node = NULL;

#ifndef DBG_EN
#define DBG_EN 0
#endif

#if DBG_EN
#define DBG_VAR_NUM 24
uint16_t dbg_p[DBG_VAR_NUM];
uint8_t dbg_idx = 0;
#endif

uint16_t gus_trap_line;

void set_steady_state(uint8_t led_num, uint8_t val)
{
    // Val 0 ==> On ==> output 1
    // Val 1 ==> Off ==> output 0
    switch(led_num) {
        case 0:
            // OUT_LED0_0 ==> OC0A (PINB2)
            PORTB = val ? (PORTB | OUT_LED0_0_MASK) : (PORTB & ~OUT_LED0_0_MASK);
            TCCR0A &= ~((1 << COM0A1) | (1 << COM0A0));
            break;

        case 1:
            // OUT_LED0_1 ==> OC1A (PINB3)
            PORTB = val ? (PORTB | OUT_LED0_1_MASK) : (PORTB & ~OUT_LED0_1_MASK);
            TCCR1A &= ~((1 << COM1A1) | (1 << COM1A0));
            break;

        case 2:
            // OUT_SW0_2 ==> OC1B (PINB4)
            PORTB = val ? (PORTB | OUT_LED0_2_MASK) : (PORTB & ~OUT_LED0_2_MASK);
            TCCR1A &= ~((1 << COM1B1) | (1 << COM1B0));
            break;

        case 3:
            // OUT_SW1_3 ==> OC0B (PIND5)
            PORTD = val ? (PORTD | OUT_LED1_3_MASK) : (PORTD & ~OUT_LED1_3_MASK);
            TCCR0A &= ~((1 << COM0B1) | (1 << COM0B0));
            break;
    }
}

void set_pwm(uint8_t led_num, uint8_t pwm_val)
{
    uint8_t volatile *pwm_reg_ptr = NULL;

    // Select PWM register
    switch(led_num) {
        case 0:
            TCCR0A |= (1 << COM0A1);   // Clear OC0A on Compare Match
            pwm_reg_ptr = &OCR0A;
            break;
        case 1:
            TCCR1A |= (1 << COM1A1);
            pwm_reg_ptr = &OCR1AL;
            break;
        case 2:
            TCCR1A |= (1 << COM1B1);
            pwm_reg_ptr = &OCR1BL;
            break;
        case 3:
            TCCR0A |= (1 << COM0B1);
            pwm_reg_ptr = &OCR0B;
            break;
    }

    // Avoid too small changes in order to avoid flickering
    if (pwm_reg_ptr && abs(pwm_val - *pwm_reg_ptr) > 2) {
        *pwm_reg_ptr = pwm_val;
    }

}


void switch_on_rx(uint8_t idx, const uint8_t *buf_in)
{
    UNREFERENCED_PARAM(idx);
    uint8_t len = buf_in[NLINK_HDR_OFF_LEN];

    if (buf_in[NLINK_HDR_OFF_TYPE] == NODE_TYPE_SWITCH) {
        // Switch event

        // Sanity check
        if (len > SWITCHES_NUM)
            return;

        for (uint8_t i = 0; i < len; i++) {
            uint8_t sw_event = buf_in[NLINK_HDR_OFF_DATA + i] & 0x0F;
            uint8_t sw_num = buf_in[NLINK_HDR_OFF_DATA + i] >> 4;
            sw_behavior_control(sw_num, sw_event);
        }

    } else {
        // Unexpected event type
        return;
    }
}

int main(void)
{
    // Wait a little just in case
    for(uint8_t uc_i = 0; uc_i < 255U; uc_i++){

        __asm__ __volatile__ ("    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"
                                ::);
    }

    DDRB = 0;

    // Out switches - output 0
    DDRB |= OUT_LED0_MASK;
    PORTB &= ~OUT_LED0_MASK;

    DDRD |= OUT_LED1_MASK;
    PORTD &= ~OUT_LED1_MASK;

    // Set timer 0 & 1 to 8-bit Fast PWM, no prescaler
    TCCR0A = (1 << WGM00) | (1 << WGM01);
    TCCR0B = (1 << CS00);

    TCCR1A = (1 << WGM10);
    TCCR1B = (1 << WGM12) | (1 << CS10);

    // Enable Timer0 overflow interrupt
    TIMSK = (1<<TOIE0);

    // NLINK configuration & init
    PCMSK |= NLINK_IO_RX_PIN_MASK;  // Enable Pin Change interrupt
    ha_nlink_init();

    ha_node_ledlight_init();
    ha_node_switch_init();

    sei();

    // Endless loop
    while(1)
    {
        __asm__ __volatile__ ("    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"
                                ::);
        ha_nlink_check_rx();
        ha_nlink_check_tx();
        if (guc_led_intenisity_timer) {
            guc_led_intenisity_timer = 0;
            ha_node_ledlight_on_timer();
        }
        if (guc_switches_timer) {
            guc_switches_timer = 0;
            ha_node_switch_on_timer();
        }
#if 0
// TODO: finalize code below
        if (guc_eeprom_save_flags) {
            for (uint8_t uc_led = 0; uc_led < LEDS_NUM; uc_led++) {
                eeprom_write_byte(&euca_led_disabled[uc_led], gta_leds[uc_led].uc_disabled);
            }
            for (uc_led = 0; uc_led < LEDS_NUM; uc_led++) {
                eeprom_write_byte(&euca_on_intensity[uc_led], gta_leds[uc_led].uc_on_intensity);
            }
        }
#endif
    }
}

void ha_node_ledlight_on_event(uint8_t uc_led, enum LED_MODE uc_mode) {
    // 0x00 leds in transition
    // 0xFF leds in steady state
    if ( (uc_mode == LED_MODE_ON) || (uc_mode == LED_MODE_OFF) ) {
        // Leds are steady - enable all switches
        guc_sw_event_mask = 0xFF;
    } else {
        // Leds are in transition - disable all switches
        guc_sw_event_mask = 0;
    }
    guc_curr_led_mode = uc_mode;
}

void ha_node_switch_on_event (uint8_t uc_switch, uint8_t uc_event)
{
    // No event - no action
    if (uc_event == 0)
    return;

    // Ignore event if switch masked out
    if ((guc_sw_event_mask & (1 << uc_switch)) == 0)
    return;

    // ----------------------------------------------------
    // --- LED OFF
    // ----------------------------------------------------
    if (guc_curr_led_mode == LED_MODE_OFF && uc_event) {
        if (uc_event == SW_EVENT_OFF_ON) {                              // LED OFF mode     OFF --> ON
            // Switch pressed
            light_on();
            guc_sw_event_mask = (1 << uc_switch);
        }

        else if (uc_event == SW_EVENT_ON_HOLD) {                          // LED OFF mode    ON --> HOLD
            // switch held
            led_disable_roll();
        }

        else if (uc_event == SW_EVENT_HOLD_OFF ||                         // Led OFF mode   HOLD --> OFF
        uc_event == SW_EVENT_ON_OFF) {                           // Led OFF mode   ON   --> OFF
            // switch released
            if (uc_event == SW_EVENT_HOLD_OFF) {
                // Save disabled leds in eeprom when roll finished
                guc_eeprom_save_flags |= EEPROM_SAVE_FLAG_DISABLED;
            }

            // Set current mode to TRANSITION to ON.
            // Disable all switches and wait until LEDs will
            // be in steady state again.
            // LEDs intensities were changed while switch was
            // held and may still may not reach the target value.
            guc_curr_led_mode = LED_MODE_ON_TRANS;
            guc_sw_event_mask = 0;
        }

    } // End of LED OFF state

    // ----------------------------------------------------
    // --- LED ON
    // ----------------------------------------------------
    else if (guc_curr_led_mode == LED_MODE_ON && uc_event){
        if (uc_event == SW_EVENT_OFF_ON) {                                     // LED ON state   OFF --> ON
            // switch pressed
            // Disable all switches excluding just pressed
            guc_sw_event_mask = (1 << uc_switch);
        }

        else if (uc_event == SW_EVENT_ON_HOLD) {                               // LED ON state   ON --> HOLD
            // Switch held - Dimm intensity
            for (uint8_t uc_led = 0; uc_led < LEDS_NUM; uc_led++) {
                dimm_on(uc_led);
            }
        }

        else if (uc_event == SW_EVENT_HOLD_OFF) {                             // LED ON state   HOLD --> OFF
            // switch released after HOLD
            guc_eeprom_save_flags |= EEPROM_SAVE_FLAG_INTENSITY;

            // Set current mode to TRANSITION to ON
            guc_curr_led_mode = LED_MODE_ON_TRANS;
            // Disable all switches until leds in steady state
            guc_sw_event_mask = 0;
        }

        else if (uc_event == SW_EVENT_ON_OFF) {                               // LED ON state   ON --> OFF
            // switch released after ON
            light_off();

            // Set current mode to TRANSITION to OFF
            guc_curr_led_mode = LED_MODE_OFF_TRANS;
            // Disable all switches until leds in steady state
            guc_sw_event_mask = 0;
        }

    } // End of LED ON mode

    return;
}
ISR(PCINT0_vect) {

    if ((NLINK_IO_RX_PIN & NLINK_IO_RX_PIN_MASK) == 0) {
        // Call NLINK start callback on RX pin falling edge
        isr_nlink_io_on_start_edge();
    }
}

// Interrupt triggered every 256 timer clocks and count periods
ISR(TIMER0_OVF_vect) {

    // 256 clocks @ 20 MHz ==> 12.8usec

    // Call NLINK timer callback every 256usec
    if (g_ha_nlink_timer_cnt >= 0) {
        g_ha_nlink_timer_cnt++;
        if (g_ha_nlink_timer_cnt == 20) {
            g_ha_nlink_timer_cnt = 0;
            isr_nlink_io_on_timer();
        }
    }

    // Call every ~10ms ~= 781 * 12.8 = 9996.8us = 9.99ms
    g_led_intensity_cnt++;
    if (g_led_intensity_cnt == 781) {
        g_led_intensity_cnt = 0;
        guc_led_intenisity_timer = 1;
    }

    // Call every ~10ms ~= 781 * 12.8 = 9996.8us = 9.99ms
    g_switches_cnt++;
    if (g_switches_cnt == 781) {
        g_switches_cnt = 0;
        guc_switches_timer = 1;
    }

}
