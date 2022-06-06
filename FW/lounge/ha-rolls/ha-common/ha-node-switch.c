/*
 * ha_switch.c
 *
 * Created: 4/30/2018 4:37:10 PM
 *  Author: Solit
 */
#include <stddef.h>
#include <stdlib.h>

#include <avr/eeprom.h>

#include "ha-common.h"
#include "ha-nlink.h"
#include "ha-node-switch.h"

#define SW_DEBOUNCE_TIMER     5  // in 10ms ticks
#define SW_HOLD_TIMER       100  // in 10ms ticks
#define SW_HOLD_TIMER_NEXT  150  // in 10ms ticks
#define SW_PIN_PRESSED  0
#define SW_PIN_RELEASED 1

#define SW_TYPE_BUTT   2

void switch_on_rx(void *ctx, const uint8_t *buf_in)
{
    UNREFERENCED_PARAM(ctx);
    UNREFERENCED_PARAM(buf_in);
    // Nothing to do here. Switch is neither
    // controllable nor configurable
}

ha_node_sw_info_t *ha_node_switch_create (const ha_node_sw_cfg_t *cfg)
{
    switch_info_t *sw;
    ha_node_sw_info_t *node_sw;
    node_t *node;
    uint8_t node_addr = eeprom_read_byte(&cfg->node_addr);
    uint8_t sw_num = eeprom_read_byte(&cfg->switches_num);

    sw = (switch_info_t*)calloc(sw_num, sizeof(switch_info_t));            /* 7B */
    node_sw = (ha_node_sw_info_t*)calloc(1, sizeof(ha_node_sw_info_t));    /* 7B */

    node = ha_nlink_node_register(node_addr, NODE_TYPE_SWITCH, switch_on_rx, NULL);

    node_sw->cfg = cfg;
    node_sw->switches_num = sw_num;
    node_sw->sw = sw;
    node_sw->node = node;
    
    // --- SWITCHES initialize ---
    for (uint8_t uc_i = 0; uc_i < node_sw->switches_num; uc_i ++) {
        sw[uc_i].uc_prev_pin = SW_PIN_RELEASED;
        sw[uc_i].uc_prev_sw = SW_PIN_RELEASED;
        sw[uc_i].uc_hold_timer = 0;
        sw[uc_i].uc_debounce_timer = 0;
        sw[uc_i].uc_pin_mask = _BV(uc_i);
        sw[uc_i].uc_switch_type = SW_TYPE_BUTT;       // mark all switches as a button
    }

// SWITCH DATA
//      TYPE(SWITCH) EVENT(%)
//
    node->tx_buf[NLINK_HDR_OFF_LEN] = 0;
    node->tx_buf[NLINK_HDR_OFF_TYPE] = NODE_TYPE_SWITCH;
    
    return node_sw;
}


void ha_node_switch_on_timer(ha_node_sw_info_t *node_sw)
{// +5 +2
    uint8_t  uc_i;
    uint8_t  uc_sw_state;
    uint8_t  uc_sw_pins;
    uint8_t  uc_curr_pin;

    uc_sw_pins = ha_node_switch_get_pins();

    for (uc_i = 0; uc_i < node_sw->switches_num; uc_i++) { 
        switch_info_t *sw = &node_sw->sw[uc_i];

        uc_sw_state = sw->uc_prev_sw;
        uc_curr_pin = !!(uc_sw_pins & sw->uc_pin_mask);

#if 1
        // ------------------------------------
        // --- debouncing
        // -----------------------------------
        if (uc_curr_pin != sw->uc_prev_pin) {
            // current pin state differs from previous
            // increment debounce timer
            sw->uc_debounce_timer ++;

            if (sw->uc_debounce_timer == SW_DEBOUNCE_TIMER) {
                // debounce timer expired
                sw->uc_prev_pin = uc_curr_pin;

                // modify current switch state (pressed/released)
                uc_sw_state = uc_curr_pin;
                sw->uc_debounce_timer = 0;
            }
        }
        else {
            sw->uc_debounce_timer = 0;
        }
#endif
        // ------------------------------------
        // --- transition proceed
        // -----------------------------------
        sw->uc_event = SW_EVENT_NONE;

        if (uc_sw_state == SW_PIN_PRESSED && sw->uc_prev_sw == SW_PIN_RELEASED) {
            // released->pressed transition
            // clear hold timer
            sw->uc_hold_timer = 0;
            sw->uc_event = SW_EVENT_OFF_ON;                                    // ---         OFF -> ON       ---
        }
        else if (uc_sw_state == SW_PIN_RELEASED && sw->uc_prev_sw == SW_PIN_PRESSED) {
            // pressed->released transition
            if (sw->uc_hold_timer < SW_HOLD_TIMER){
                sw->uc_event = SW_EVENT_ON_OFF;                                // ---         ON -> OFF        ---
            }
            else {
                sw->uc_event = SW_EVENT_HOLD_OFF;                              // ---         HOLD -> OFF      ---
            }
        }
        else if (uc_sw_state == SW_PIN_PRESSED && sw->uc_prev_sw == SW_PIN_PRESSED) {
            if (sw->uc_hold_timer < SW_HOLD_TIMER_NEXT) {
                // hold timer not saturated yet
                // increase hold timer & check saturation
                sw->uc_hold_timer ++;

                if (sw->uc_hold_timer == SW_HOLD_TIMER) {
                    // report "switch hold" if timer saturated
                    sw->uc_event = SW_EVENT_ON_HOLD;                           // ---         ON -> HOLD      ---
                }

                if (sw->uc_hold_timer == SW_HOLD_TIMER_NEXT) {
                    // report "switch hold" if timer saturated
                    sw->uc_event = SW_EVENT_ON_HOLD;                           // ---         ON -> HOLD NEXT ---
                    sw->uc_hold_timer = SW_HOLD_TIMER;
                }
            }
        } // End of switch held

        sw->uc_prev_sw = uc_sw_state;

    } // End of switch loop

    // Check events - if any occur then send event
    node_t *node = node_sw->node;
    uint8_t len = 0;
#if 1
     for (uc_i = 0; uc_i < node_sw->switches_num; uc_i++) {
        switch_info_t *sw = &node_sw->sw[uc_i];
        if (sw->uc_event != SW_EVENT_NONE) {
            node->tx_buf[NLINK_HDR_OFF_DATA + len] = (uc_i << 4) | sw->uc_event;
            sw->uc_event = SW_EVENT_NONE;
            len ++;
        }
    }
#endif    

    if (len) {
        node->tx_buf[NLINK_HDR_OFF_LEN] = len;
        ha_nlink_node_send(node, NODE_ADDR_BC, NLINK_CMD_INFO);
        // TODO: configure the destination address ^^^ as a param
    }
}
