/*
 * ha_switch.c
 *
 * Created: 4/30/2018 4:37:10 PM
 *  Author: Solit
 */
 // TODO:
 // 1. Make HW pins independent through include of #ifdef
#include <avr/io.h>
#include <stdint.h>
#include <stddef.h>
#include "ha-common.h"
#include "ha-nlink.h"
#include "ha-node-switch.h"

node_t *g_switch_nlink_node = (void*)0;
uint8_t guc_sw_event_mask;

typedef struct {
    uint8_t uc_event;         // OFF_ON, ON_OFF, ON_HOLD, HOLD_OFF
    uint8_t uc_prev_pin;      // 0/1 not debounced
    uint8_t uc_prev_sw;       // pressed/released debounced
    uint8_t uc_hold_timer;
    uint8_t uc_debounce_timer;
    uint8_t uc_pin_mask;
    uint8_t uc_switch_type;   // button/movement detector
}SWITCH_INFO;

#define SW_PORT  PORTA
#define SW_DIR   DDRA
#define SW_PIN   PINA

#define SW_PIN0   PINA0
#define SW_PIN2   PINA2
#define SW_PIN3   PINA3
#define SW_PIN4   PINA4
#define SW_PIN5   PINA5

#define SW_MASK_ALL ((1 << SW_PIN0) |\
                     (1 << SW_PIN2) |\
                     (1 << SW_PIN3) |\
                     (1 << SW_PIN4) |\
                     (1 << SW_PIN5))

#define SW_DEBOUNCE_TIMER     5  // in 10ms ticks
#define SW_HOLD_TIMER       100  // in 10ms ticks
#define SW_HOLD_TIMER_NEXT  150  // in 10ms ticks
#define SW_PIN_PRESSED  0
#define SW_PIN_RELEASED 1

#define SW_TYPE_BUTT   2

#define SWITCHES_NUM   5

uint8_t gta_switches_pin_masks[SWITCHES_NUM] = {
    _BV(SW_PIN0),
    _BV(SW_PIN2),
    _BV(SW_PIN3),
    _BV(SW_PIN4),
    _BV(SW_PIN5),
};

SWITCH_INFO gta_switches[SWITCHES_NUM];

void switch_on_rx(uint8_t idx, const uint8_t *buf_in)
{
    UNREFERENCED_PARAM(idx);
    UNREFERENCED_PARAM(buf_in);
    // Nothing to do here. Switch is neither
    // controllable nor configurable
}

void ha_node_switch_init()
{
    // Set switch pins to pull-up inputs
    SW_DIR  &= ~SW_MASK_ALL;
    SW_PORT |= SW_MASK_ALL;

    // Disable all event until light is in steady state after power on
    guc_sw_event_mask = 0;

    // --- SWITCHES initialize ---
    for (uint8_t uc_i = 0; uc_i < SWITCHES_NUM; uc_i ++)
    { // loop over all switches
        gta_switches[uc_i].uc_prev_pin = SW_PIN_RELEASED;
        gta_switches[uc_i].uc_prev_sw = SW_PIN_RELEASED;
        gta_switches[uc_i].uc_hold_timer = 0;
        gta_switches[uc_i].uc_debounce_timer = 0;
        gta_switches[uc_i].uc_pin_mask = gta_switches_pin_masks[uc_i];
        gta_switches[uc_i].uc_switch_type = SW_TYPE_BUTT;       // mark all switches as a button
    }

// SWITCH DATA
//      TYPE(SWITCH) EVENT(%)
//
    g_switch_nlink_node = ha_nlink_node_register(SWITCH_ADDR, NODE_TYPE_SWITCH, switch_on_rx, (node_tx_cb_t)NULL);

    // Clear TX buffer
    node_t *node = g_switch_nlink_node;
    node->tx_buf[NLINK_HDR_OFF_LEN] = 0;
    node->tx_buf[NLINK_HDR_OFF_TYPE] = NODE_TYPE_SWITCH;
}


void ha_node_switch_on_timer()
{
    uint8_t  uc_i;
    uint8_t  uc_sw_state;
    uint8_t  uc_sw_pins;
    uint8_t  uc_curr_pin;

    uc_sw_pins = SW_PIN;

    for (uc_i = 0; uc_i < SWITCHES_NUM; uc_i++)
    { // Loop over all switches

        uc_sw_state = gta_switches[uc_i].uc_prev_sw;
        uc_curr_pin = !!(uc_sw_pins & gta_switches[uc_i].uc_pin_mask);

        // ------------------------------------
        // --- debouncing
        // -----------------------------------
        if (uc_curr_pin != gta_switches[uc_i].uc_prev_pin)
        {// current pin state differs from previous

            // increment debounce timer
            gta_switches[uc_i].uc_debounce_timer ++;

            if (gta_switches[uc_i].uc_debounce_timer == SW_DEBOUNCE_TIMER)
            { // debounce timer expired
                gta_switches[uc_i].uc_prev_pin = uc_curr_pin;

                // modify current switch state (pressed/released)
                uc_sw_state = uc_curr_pin;
                gta_switches[uc_i].uc_debounce_timer = 0;

            }
        }
        else
        {
            gta_switches[uc_i].uc_debounce_timer = 0;
        }

        // ------------------------------------
        // --- transition proceed
        // -----------------------------------
        gta_switches[uc_i].uc_event = 0;

        if (uc_sw_state == SW_PIN_PRESSED && gta_switches[uc_i].uc_prev_sw == SW_PIN_RELEASED)
        { // released->pressed transition
            // clear hold timer
            gta_switches[uc_i].uc_hold_timer = 0;
            gta_switches[uc_i].uc_event = SW_EVENT_OFF_ON;                                    // ---         OFF -> ON       ---
        }
        else if (uc_sw_state == SW_PIN_RELEASED && gta_switches[uc_i].uc_prev_sw == SW_PIN_PRESSED )
        { // pressed->released transition
            if (gta_switches[uc_i].uc_hold_timer < SW_HOLD_TIMER)
            {
                gta_switches[uc_i].uc_event = SW_EVENT_ON_OFF;                                // ---         ON -> OFF        ---
            }
            else
            {
                gta_switches[uc_i].uc_event = SW_EVENT_HOLD_OFF;                              // ---         HOLD -> OFF      ---
            }
        }
        else if (uc_sw_state == SW_PIN_PRESSED && gta_switches[uc_i].uc_prev_sw == SW_PIN_PRESSED)
        {
            if (gta_switches[uc_i].uc_hold_timer < SW_HOLD_TIMER_NEXT)
            { // hold timer not saturated yet

                // increase hold timer & check saturation
                gta_switches[uc_i].uc_hold_timer ++;

                if (gta_switches[uc_i].uc_hold_timer == SW_HOLD_TIMER)
                {// report "switch hold" if timer saturated
                    gta_switches[uc_i].uc_event = SW_EVENT_ON_HOLD;                           // ---         ON -> HOLD      ---
                }

                if (gta_switches[uc_i].uc_hold_timer == SW_HOLD_TIMER_NEXT)
                {// report "switch hold" if timer saturated
                    gta_switches[uc_i].uc_event = SW_EVENT_ON_HOLD;                           // ---         ON -> HOLD NEXT ---
                    gta_switches[uc_i].uc_hold_timer = SW_HOLD_TIMER;
                }
            }
        } // End of switch held

        gta_switches[uc_i].uc_prev_sw = uc_sw_state;

    } // End of switch loop

    // Check events - if any occur then update LED node
    node_t *node = g_switch_nlink_node;
    uint8_t len = 0;
    for (uc_i = 0; uc_i < SWITCHES_NUM; uc_i++) {
        if (gta_switches[uc_i].uc_event) {
            node->tx_buf[NLINK_HDR_OFF_DATA + len] = (uc_i << 4) | gta_switches[uc_i].uc_event;
            gta_switches[uc_i].uc_event = 0;
            len ++;
        }
    }
    // TODO: LEDLIGHT ADDR might be configurable and stored in EEPROM
    if (len) {
        node->tx_buf[NLINK_HDR_OFF_LEN] = len;
        ha_nlink_node_send(node, LEDLIGHT_ADDR, NLINK_CMD_INFO);
    }
}
