#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <stdlib.h>
#include <string.h>
#include "ha-common.h"
//#include "ha-ledlight.h"
//#include "ha-ledlight_const.h"
#include "ha-nlink.h"
#include "ha-node-ledlight.h"

/*
* ha-ledlight.c
*
* Created: 17/03/2019 9:49:58 AM
*  Author: Solit
*/

LED_INFO gta_leds[LEDS_NUM];

uint8_t  guc_leds_disabled_idx;
uint8_t  guc_leds_disabled_mask;
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

void ha_node_ledlight_on(){
    uint8_t led_mask = 1;

    for (uint8_t uc_i = 0; uc_i < LEDS_NUM; uc_i ++, led_mask <<= 1) {
        LED_INFO *led = &gta_leds[uc_i];
        led->uc_target_intensity_idx =  led_mask & guc_leds_disabled_mask ? 0 : led->uc_on_intensity_idx;
        led->uc_fade_timer = LED_FADEIN_PERIOD;
    }

    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_MODE] = LED_MODE_ON;        // mode is not ON yet, but will be soon
    ha_nlink_node_send(g_ll_node, NODE_ADDR_BC, NLINK_CMD_INFO);
}

void ha_node_ledlight_off(){

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
            gta_leds[uc_i].uc_on_intensity_idx--;
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
    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY3] = gta_leds[3].uc_on_intensity_idx;
    ha_node_ledlight_on();
}

void led_disable_roll(){
    guc_leds_disabled_idx++;
    if (guc_leds_disabled_idx == ROLL_ON_MASK_NUM) guc_leds_disabled_idx = 0;
    guc_leds_disabled_mask = guca_disable_roll_table[guc_leds_disabled_idx];
    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_DIS_MASK ] = guc_leds_disabled_mask;
    ha_node_ledlight_on();
}

void ledlight_on_rx(uint8_t idx, const uint8_t *buf_in)
{
    UNREFERENCED_PARAM(idx);
    uint8_t len = buf_in[NLINK_HDR_OFF_LEN];

    if (buf_in[NLINK_HDR_OFF_TYPE] == NODE_TYPE_LEDLIGHT) {
        // Direct LEDLIGHT state info. Typically received from user console
        guc_curr_led_mode = buf_in[NLINK_HDR_OFF_DATA + LL_DATA_MODE];
        guc_leds_disabled_mask = buf_in[NLINK_HDR_OFF_DATA + LL_DATA_DIS_MASK];
        gta_leds[0].uc_on_intensity_idx = buf_in[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY0];
        gta_leds[1].uc_on_intensity_idx = buf_in[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY1];
        gta_leds[2].uc_on_intensity_idx = buf_in[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY2];
        gta_leds[3].uc_on_intensity_idx = buf_in[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY3];

        // Update node TX buffer to reflect changes in outgoing messages
        memcpy(&g_ll_node->tx_buf[NLINK_HDR_OFF_DATA], &buf_in[NLINK_HDR_OFF_DATA], LL_DATA_LEN);

        if (guc_curr_led_mode == LED_MODE_ON) {
            ha_node_ledlight_on();
        } else if (guc_curr_led_mode == LED_MODE_OFF) {
            ha_node_ledlight_off();
        }

    } else {
        // Unexpected event type
        return;
    }
}

void ha_node_ledlight_init()
{
    node_t *node = ha_nlink_node_register(LEDLIGHT_ADDR, NODE_TYPE_LEDLIGHT, ledlight_on_rx, NULL);
    g_ll_node = node;


    // Switch LEDs off on startup
    ha_node_ledlight_on_event(0xFF, LED_MODE_OFF_TRANS);
    guc_curr_led_mode = LED_MODE_OFF_TRANS;
    guc_leds_steady = 0;

    guc_leds_disabled_idx = 0;                      // All Enabled
    guc_leds_disabled_mask = guca_disable_roll_table[guc_leds_disabled_idx];
    gta_leds[0].uc_on_intensity_idx = INTENSITIES_NUM - 1;     // Max;
    gta_leds[1].uc_on_intensity_idx = INTENSITIES_NUM - 1;     // Max;
    gta_leds[2].uc_on_intensity_idx = INTENSITIES_NUM - 1;     // Max;
    gta_leds[3].uc_on_intensity_idx = INTENSITIES_NUM - 1;     // Max;

    node->tx_buf[NLINK_HDR_OFF_TYPE] = NODE_TYPE_LEDLIGHT;
    node->tx_buf[NLINK_HDR_OFF_LEN] = LL_DATA_LEN;
    // node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_MODE     ] = LED_MODE_OFF;     // Is set unconditionally on every packet send
    node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_DIS_MASK ] = guc_leds_disabled_mask;

    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY0] = gta_leds[0].uc_on_intensity_idx;
    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY1] = gta_leds[1].uc_on_intensity_idx;
    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY2] = gta_leds[2].uc_on_intensity_idx;
    g_ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_INTENSITY3] = gta_leds[3].uc_on_intensity_idx;

    ha_node_ledlight_off();
}

void ha_node_ledlight_on_timer()
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
            ha_node_ledlight_on_event(0xFF, LED_MODE_ON);
            guc_curr_led_mode = LED_MODE_ON;
        }
    } // End of TRANSITION to ON

    else if (guc_curr_led_mode == LED_MODE_OFF_TRANS) {
        if (guc_leds_steady) {
            ha_node_ledlight_on_event(0xFF, LED_MODE_OFF);
            guc_curr_led_mode = LED_MODE_OFF;
        }
        return;
    } // End of TRANSITION to OFF

    return;
}

