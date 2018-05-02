/*
 * ha_node_hvac.h
 *
 * Created: 5/1/2018 11:44:16 PM
 *  Author: Solit
 */ 

#pragma once

#define HVAC_RELAYS_MASK_MOTOR_I    _BV(0)
#define HVAC_RELAYS_MASK_MOTOR_II   _BV(1)
#define HVAC_RELAYS_MASK_VALVE      _BV(2)

typedef struct hvac_s {
    uint8_t hvac_node_h;
    uint8_t relays; // bit mask
    uint8_t heater; // 0..100%
} hvac_t;



extern hvac_t hvac;

#if 0
hvac_t hvac;

void hvac_on_rx(uint8_t idx, const uint8_t *buf_in) 
{
#define HVAC_OFFSET_RELAYS      0
#define HVAC_OFFSET_HEATER      1
#define HVAC_OFFSET_TEMP        2
#define HVAC_OFFSET_PRESSURE    3
#define HVAC_OFFSET_HUMIDITY    4

    hvac.relays = buf_in[NLINK_HDR_OFF_DATA + HVAC_OFFSET_RELAYS];
    hvac.heater = buf_in[NLINK_HDR_OFF_DATA + HVAC_OFFSET_HEATER];

    // ...
    // Apply control 
    // ...
    
    // Update FSM
    // ...
}

#endif