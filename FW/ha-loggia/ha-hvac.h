#pragma once
#include "ha-node-hvac.h"
#include "ha-phts.h"

#define HVAC_HEATER_CONTROL_PIN     PIND
#define HVAC_HEATER_CONTROL_PORT    PORTD
#define HVAC_HEATER_CONTROL_DIR     DDRD
#define HVAC_HEATER_CONTROL_MASK    _BV(PIND5)   // OC0B

#define HVAC_SWITCH_VALVE   PINB3
#define HVAC_SWITCH_MOTOR   PINB2
#define HVAC_SWITCH_MOTOR2  PINB2
#define HVAC_SWITCH_HEATER  PINB4

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

#define HVAC_TIMER_PERIOD_MS	10
#define HVAC_TIMER_SEC  (1000 / HVAC_TIMER_PERIOD_MS)
#define HVAC_TIMER_VALVE_GUARD      (5  * HVAC_TIMER_SEC)    // Is used as a guard time for valve open/close to save time during to fast state changes.
                                                             // OPEN timer is set to (FULL_TIME - CURR_CLOSE_TIME) and vice versa

#define HVAC_TIMER_VALVE_OPEN       (75 * HVAC_TIMER_SEC)    // Full actuator running time = 150s (RDAB5-230). 50% should be enough to switch to next/prev state
#define HVAC_TIMER_VALVE_CLOSE      HVAC_TIMER_VALVE_OPEN

#define HVAC_TIMER_MOTOR_START      (2  * HVAC_TIMER_SEC)  // Time
#define HVAC_TIMER_HEATER_COOLDOWN  (30 * HVAC_TIMER_SEC)  // Time while heater cools down till temperature safe enough to switch off motor

typedef struct hvac_s {
    uint8_t state_trgt;
    uint8_t state_trgt_prev;

    uint8_t state_curr;
    uint16_t state_timer_p;
    uint16_t state_timer_m;

    uint8_t heater_ctrl_mode;   // Automatic or manual temperature control
    uint8_t heater_ctrl_trgt;   // Fixed value set by user or reg
    uint8_t heater_ctrl_curr;   // Current PWM value set

    node_t   *node;
    ha_phts_t   sensor;
    union pt_prom_u sensor_pt_prom;

    uint8_t sensor_prev_rd_cnt;
    uint8_t sensor_prev_state;
    uint8_t sensor_prev_prom_rd_idx;
    uint8_t sensor_temperature;
    uint8_t sensor_pressure;
} hvac_t;

extern void ha_hvac_fsm();
extern void ha_hvac_init();