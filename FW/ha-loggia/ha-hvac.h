#pragma once
#include "ha-node-hvac.h"
#include "ha-phts.h"

#define HVAC_HEATER_CONTROL_PIN     PIND
#define HVAC_HEATER_CONTROL_PORT    PORTD
#define HVAC_HEATER_CONTROL_DIR     DDRD
#define HVAC_HEATER_CONTROL_MASK    _BV(PIND5)   // OC0B

#define HVAC_SWITCH_VALVE   PINB4
#define HVAC_SWITCH_MOTOR   PINB3
#define HVAC_SWITCH_MOTOR2  PINB2
#define HVAC_SWITCH_HEATER  PINB1

#define HVAC_SWITCH_MASK ( \
    _BV(HVAC_SWITCH_VALVE  ) |\
    _BV(HVAC_SWITCH_MOTOR  ) |\
    _BV(HVAC_SWITCH_MOTOR2 ) |\
    _BV(HVAC_SWITCH_HEATER ))

#define HVAC_SWITCH_STATE_S4_M1 (\
    _BV(HVAC_SWITCH_VALVE  ) |\
    _BV(HVAC_SWITCH_MOTOR  ) |\
    _BV(HVAC_SWITCH_HEATER ))

#define HVAC_SWITCH_STATE_S4_M2 (\
    _BV(HVAC_SWITCH_VALVE  ) |\
    _BV(HVAC_SWITCH_MOTOR  ) |\
    _BV(HVAC_SWITCH_MOTOR2 ) |\
    _BV(HVAC_SWITCH_HEATER ))

#define HVAC_SWITCH_STATE_S3_M1 (\
    _BV(HVAC_SWITCH_VALVE  ) |\
    _BV(HVAC_SWITCH_MOTOR  ))

#define HVAC_SWITCH_STATE_S3_M2 (\
    _BV(HVAC_SWITCH_VALVE  ) |\
    _BV(HVAC_SWITCH_MOTOR  ) |\
    _BV(HVAC_SWITCH_MOTOR2 ))

#define HVAC_SWITCH_STATE_S2 (\
    _BV(HVAC_SWITCH_VALVE  ))

#define HVAC_SWITCH_STATE_S1 0


#define HVAC_SWITCH_PORT PORTB
#define HVAC_SWITCH_PIN  PINB
#define HVAC_SWITCH_DIR  DDRB

// MS860702BA01
#define HVAC_PHTS_I2C_ADDR_PT   0x76 // Pressure and Temperature
#define HVAC_PHTS_I2C_ADDR_RH   0x40 // Relative Humidity RH

#define HVAC_TIMER_SEC  100
#define HVAC_TIMER_VALVE_OPEN       (10 * HVAC_TIMER_SEC)  // TODO: TBD. Time while motor rotates valve from one state to another
#define HVAC_TIMER_VALVE_CLOSE      HVAC_TIMER_VALVE_OPEN

#define HVAC_TIMER_MOTOR_START      (2  * HVAC_TIMER_SEC)  // Time
#define HVAC_TIMER_HEATER_COOLDOWN  (30 * HVAC_TIMER_SEC)  // Time while heater cools down till temperature safe enough to switch off motor

typedef struct hvac_s {
    uint8_t state_trgt;
    uint8_t state_curr;
    uint16_t state_timer;

    uint8_t heater_ctrl_mval;  // (MODE << 7)| Fixed value set by user or reg
    uint8_t heater_ctrl_curr;  // Current PWM value set

    node_t   *node;
    phts_t   sensor;
} hvac_t;

extern void ha_hvac_fsm();
extern void ha_hvac_init();