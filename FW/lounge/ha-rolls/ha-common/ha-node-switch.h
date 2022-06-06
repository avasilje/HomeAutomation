/*
 * ha-node-switch.h
 *
 * Created: 4/30/2018 4:37:47 PM
 *  Author: Solit
 */

#pragma once

enum sw_event_e {
    SW_EVENT_NONE     = 0,
    SW_EVENT_ON_OFF   = 1,
    SW_EVENT_OFF_ON   = 2,
    SW_EVENT_ON_HOLD  = 3,
    SW_EVENT_HOLD_OFF = 4,
};

typedef struct {
    enum sw_event_e uc_event; // OFF_ON, ON_OFF, ON_HOLD, HOLD_OFF . Att: 4Bits!
    uint8_t uc_prev_pin;      // 0/1 not debounced
    uint8_t uc_prev_sw;       // pressed/released debounced
    uint8_t uc_hold_timer;
    uint8_t uc_debounce_timer;
    uint8_t uc_pin_mask;
    uint8_t uc_switch_type;   // button/movement detector
} switch_info_t;

typedef struct ha_node_sw_cfg_s {
    uint8_t node_addr;
    uint8_t switches_num;
} ha_node_sw_cfg_t;

typedef struct ha_node_sw_info_s {
    const ha_node_sw_cfg_t *cfg;    /* Att: EEMEM */
    node_t *node;
    uint8_t switches_num;
    switch_info_t *sw;
} ha_node_sw_info_t;


extern ha_node_sw_info_t *ha_node_switch_create (const ha_node_sw_cfg_t *cfg);
extern void ha_node_switch_on_timer(ha_node_sw_info_t *node_sw);

/************************************************************************/
/* Need to be implemented by the integration                            */
/************************************************************************/
extern uint8_t ha_node_switch_get_pins();

