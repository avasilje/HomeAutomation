#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <stdlib.h>
#include "ha-common.h"
#include "ha-ledlight.h"
#include "ha-ledlight_const.h"
#include "ha-nlink.h"
#include "ha-node-switch.h"
#include "ha-node-ledlight.h"

/*
* ha-ledlight.c
*
* Created: 05/05/2018 9:49:58 AM
*  Author: Solit
*/
uint16_t g_led_intensity_cnt = 0;
uint8_t guc_led_intenisity_timer = 0;
int8_t g_ha_nlink_timer_cnt = -1;

// ------- EEPROM ---------------
uint8_t euca_signature[2] __attribute__ ((section (".config_param")));
uint8_t euca_standby_intensity[LEDS_NUM] __attribute__ ((section (".config_param")));
uint8_t euca_on_intensity[LEDS_NUM] __attribute__ ((section (".config_param")));

uint8_t euc_leds_disabled_idx __attribute__ ((section (".config_param")));

LED_INFO gta_leds[LEDS_NUM];

uint8_t  guc_leds_disabled_idx;
uint8_t  guc_sw_event_mask;     //
uint8_t  guc_curr_led_mode;     //
uint8_t  guc_leds_steady;       // All LEDs are in steady state (flag)

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
            // OUT_SW0_0 PINB2  ==> OC0A
            PORTB = val ? (PORTB | OUT_SW0_0_MASK) : (PORTB & ~OUT_SW0_0_MASK);
            TCCR0A &= ~((1 << COM0A1) | (1 << COM0A0));
            break;

        case 1:
            // OUT_SW0_1 ==> OC1A
            PORTB = val ? (PORTB | OUT_SW0_1_MASK) : (PORTB & ~OUT_SW0_1_MASK);
            TCCR1A &= ~((1 << COM1A1) | (1 << COM1A0));
            break;

        case 2:
            // OUT_SW0_2 ==> OC1B
            PORTB = val ? (PORTB | OUT_SW0_2_MASK) : (PORTB & ~OUT_SW0_2_MASK);
            TCCR1A &= ~((1 << COM1B1) | (1 << COM1B0));
            break;

        case 3:
            // OUT_SW1_3 ==> OC0B
            PORTD = val ? (PORTD | OUT_SW1_3_MASK) : (PORTD & ~OUT_SW1_3_MASK);
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
void set_intensity (uint8_t led_num, uint8_t intensity_idx)
{
    if (intensity_idx >= INTENSITIES_NUM) intensity_idx = INTENSITIES_NUM - 1;

    if (intensity_idx == 0) {
        set_steady_state(led_num, 0);
    } else if (intensity_idx == INTENSITIES_NUM - 1) {
        set_steady_state(led_num, 1);
    } else {
        set_pwm(led_num, guca_pwm_intensity_table[intensity_idx]);
    }

    gta_leds[led_num].uc_current_intensity_idx = intensity_idx;
}

void light_on(){
    uint8_t led_mask = 1;
    uint8_t disabled_mask = guca_disable_roll_table[guc_leds_disabled_idx];

    for (uint8_t uc_i = 0; uc_i < LEDS_NUM; uc_i ++, led_mask <<= 1) {
        LED_INFO *led = &gta_leds[uc_i];
        led->uc_target_intensity_idx =  led_mask & disabled_mask ? 0 : led->uc_on_intensity_idx;
        led->uc_fade_timer = LED_FADEIN_PERIOD;
    }

    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_MODE] = LED_MODE_ON;        // mode is not ON yet, but will be soon
    ha_nlink_node_send(g_ll_node, NODE_ADDR_BC, NLINK_CMD_INFO);
}

void light_off(){

    for (uint8_t uc_i = 0; uc_i < LEDS_NUM; uc_i ++) {
        gta_leds[uc_i].uc_target_intensity_idx = 0;
        gta_leds[uc_i].uc_fade_timer = LED_FADEOUT_PERIOD;
    }
    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_MODE] = LED_MODE_OFF;        // mode is not OFF yet, but will be soon
    ha_nlink_node_send(g_ll_node, NODE_ADDR_BC, NLINK_CMD_INFO);
}

void dimm_on(){

    uint8_t all_off = 0;
    for (uint8_t uc_i = 0; uc_i < LEDS_NUM; uc_i ++) {
        if (gta_leds[uc_i].uc_on_intensity_idx == 0) {
            all_off ++;
        } else {
            gta_leds[uc_i].uc_on_intensity_idx--
        }
        gta_leds[uc_i].uc_fade_timer = LED_FADEOUT_PERIOD;
    }

    //Wrap around intensity only when LED are OFF
    if (all_off == LEDS_NUM) {
        for (uint8_t uc_i = 0; uc_i < LEDS_NUM; uc_i ++) {
            gta_leds[uc_i].uc_on_intensity_idx = DIMM_ON_INTENSITIES_NUM - 1;
        }
    }

    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY0] = gta_leds[0].uc_on_intensity_idx;
    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY1] = gta_leds[1].uc_on_intensity_idx;
    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY2] = gta_leds[2].uc_on_intensity_idx;
    light_on();
}

void led_disable_roll(){
    guc_leds_disabled_idx++;
    if (guc_leds_disabled_idx == ROLL_ON_MASK_NUM) guc_leds_disabled_idx = 0;
    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_DIS_MASK ] = guc_leds_disabled_idx;
    light_on();
}

void sw_behavior_control(uint8_t uc_switch, uint8_t uc_event)
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

void ledlight_on_rx(uint8_t idx, const uint8_t *buf_in)
{
    UNREFERENCED_PARAM(idx);
    uint8_t len = buf_in[NLINK_HDR_OFF_LEN];
    !!!!!!!!! debug multi channel intensity from CCD !!!!!
    if (buf_in[NLINK_HDR_OFF_TYPE] == NODE_TYPE_LEDLIGHT) {
        // Direct LEDLIGHT state info. Typically received from user console
        guc_curr_led_mode = g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_MODE];
        guc_leds_disabled_idx = g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_DIS_MASK];
        gta_leds[0].uc_on_intensity_idx = g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY0];
        gta_leds[1].uc_on_intensity_idx = g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY1];
        gta_leds[2].uc_on_intensity_idx = g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY2];

        if (guc_curr_led_mode == LED_MODE_ON) {
            light_on();
        } else if (guc_curr_led_mode == LED_MODE_OFF) {
            light_off();
        }

    } else if (buf_in[NLINK_HDR_OFF_TYPE] == NODE_TYPE_SWITCH) {
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

void ha_ledlight_init()
{
    node_t *node = ha_nlink_node_register(LEDLIGHT_ADDR, NODE_TYPE_LEDLIGHT, ledlight_on_rx);
    g_ll_node = node;

    // Disable all event until light is in steady state after power on
    guc_sw_event_mask = 0;

    // Switch LEDs off on startup
    guc_curr_led_mode = LED_MODE_OFF_TRANS;
    guc_leds_steady = 0;

    guc_leds_disabled_idx = 0;                      // All Enabled
    gta_leds[0].uc_on_intensity_idx = INTENSITIES_NUM - 1;     // Max;
    gta_leds[1].uc_on_intensity_idx = INTENSITIES_NUM - 1;     // Max;
    gta_leds[2].uc_on_intensity_idx = INTENSITIES_NUM - 1;     // Max;

    node->tx_buf[NLINK_HDR_OFF_TYPE] = NODE_TYPE_LEDLIGHT;
    node->tx_buf[NLINK_HDR_OFF_LEN] = 3;
    // node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_MODE     ] = LED_MODE_OFF;
    node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_DIS_MASK ] = guc_leds_disabled_idx;

    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY0] = gta_leds[0].uc_on_intensity_idx;
    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY1] = gta_leds[1].uc_on_intensity_idx;
    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY2] = gta_leds[2].uc_on_intensity_idx;

    light_off();
}

void leds_intensity_control()
{
    uint8_t uc_target_intensity, uc_current_intensity;

    guc_leds_steady = 1;

    // Loop over all LEDs
    for (uint8_t uc_led = 0; uc_led < LEDS_NUM; uc_led ++) {

        uc_target_intensity = gta_leds[uc_led].uc_target_intensity_idx;
        uc_current_intensity = gta_leds[uc_led].uc_current_intensity_idx;

        if (uc_target_intensity == uc_current_intensity) {
            continue;
        }

        guc_leds_steady = 0;

        // LEDs' intensity are in transition
        // check fade timer
        gta_leds[uc_led].uc_fade_timer --;
        if ( gta_leds[uc_led].uc_fade_timer == 0) {
            // Its time to update fade in/out
            if (uc_target_intensity > uc_current_intensity) {
                // FADE IN
                uc_current_intensity ++;
                gta_leds[uc_led].uc_fade_timer = LED_FADEIN_STEP_PERIOD;
            }
            else if (uc_target_intensity < uc_current_intensity) {
                // FADE OUT
                uc_current_intensity --;
                gta_leds[uc_led].uc_fade_timer = LED_FADEOUT_STEP_PERIOD;
            }

            set_intensity(uc_led, uc_current_intensity);
        } // End of FADE IN/OUT

    } // End all LEDs loop

    // ----------------------------------------------
    // --- Transient states
    // ----------------------------------------------
    if (guc_curr_led_mode == LED_MODE_ON_TRANS) {
        if (guc_leds_steady) {
            guc_sw_event_mask = 0xFF;
            guc_curr_led_mode = LED_MODE_ON;
        }
    } // End of TRANSITION to ON

    else if (guc_curr_led_mode == LED_MODE_OFF_TRANS) {
        if (guc_leds_steady) {
            guc_sw_event_mask = 0xFF;
            guc_curr_led_mode = LED_MODE_OFF;
        }
        return;
    } // End of TRANSITION to OFF

    return;
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
    DDRB |= OUT_SW0_MASK;
    PORTB &= ~OUT_SW0_MASK;

    DDRD |= OUT_SW1_MASK;
    PORTD &= ~OUT_SW1_MASK;

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

    ha_ledlight_init();

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
            leds_intensity_control();
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

}
