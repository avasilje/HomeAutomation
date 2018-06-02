#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <stddef.h>
#include "ha-common.h"
#include "ha-nlink.h"
#include "ha-node-hvac.h"
#include "ha-hvac.h"
#include "ha-i2c.h"

// TODO:
//  +1. Add PWM configuration for HVAC control
//  +2. Add hvac_heater_regulation auto/manual functionality
//  +3. Bond with NLINK interface
//  4. Add I2C GPIO and reg init
//  5. Add I2C transport
//  6. Add PHTS functionality

/*
* ha-hvac.c
*
* Created: 05/05/2018 9:49:58 AM
*  Author: Solit
*/

/*
 *
 *
 *                                                           ___
 *   --> Airflow direction                                  | S | Sensor I2C
 *  INLET___                  ___                     ___    \_/  Humidity/Velocity/Temp
 * -----/ V \----------------/ M \-------------------/ H \----|------
 * -----\___/----------------\___/-------------------\___/----------- OUTLET
 *      Valve Close/Opened   Motor                    Heater
 *      Switch SPDT          Speed I/II               Power 0-100%
 *      Power - always ON    Switch1 SPST Power       Vcontrol = 0..10V (PWM)
 *                           Switch2 SPST SII On      Switch SPST Power ON/OFF
 *
 *   S1 Ventilation closed  V- M- H- S-   (Valve closed, Motor OFF, Heater OFF, Sensing: Velocity = 0)
 *   S2 Ventilation open    V+ M- H- Sv   (Valve open, Motor OFF, Heater OFF, Sensing: none)
 *   S3 Ventilation forced  V+ M+ H- Sv   (Valve open, Motor ON, Heater OFF, Sensing: Velocity > Vthrs_f)
 *   S4 Ventilation heated  V+ M+ H+ Svt  (Valve open, Motor ON, Heater ON, Sensing: Velocity > Vthrs_h; T > Tthrs_t)
 *
 * Totally 4 states.
 *     1. State can be changed in forward/backward direction only. I.e. no jump.
 *     2. State changed from current state toward target state specified by user.
 *     3. User specifies only final state. I.e. no transitional states logic on user side
 *     4. Sensors data and component's state continuously monitored and checked.
 *     5. FSM period - ~20ms
 *     6. Sensor poll timer - ~100ms
 *
 *   =============================================================================================================
 *                           |         S1           |       S2            |       S3            |      S4
 *                           |      V- M- H-        |    V+ M- H-         |     V+ M+ H-        |    V+ M+ H+
 *   ========================|======================|=====================|=====================|=================
 *                           |                      |                     |                     |
 *    On S+                  | S+ => Open Valve     | S+ => Enable Motor. | S+ => Enable Heater | prohibited
 *                           | Start(Tvo)           | Start(Tm)           | Set Vcontrol = 0    |
 *                           | Tvo (Valve Open)     | Tm = Relay Toggle + | Switch heater ON    |
 *                           | Valve switch ON      | Motor Activation    | Start(Th)           |
 *                           |                      | (1-2sec)            | Th = Relay Toggle   |
 *                           |                      | Switch Motor ON     | Set Vh = t          |
 *   ________________________|______________________|_____________________|_____________________|_________________
 *                           |                      |                     |                     |
 *    S+ allowed             | Valve can be opened  | Tvo expired         | Tm expired          | Last state
 *                           | unconditionally      |                     |                     |
 *                           |                      |                     |                     |
 *   ========================|======================|=====================|=====================|=================
 *                           |                      |                     |                     |
 *                           |                      | S- => Close valve   | S- => Disable Motor | S- => Disable Heater
 *    On S-                  | prohibited           | Start Tvc (why?)    | No timer (Tm<<Tvc)  | Start(Th)
 *                           |                      | Switch valve OFF    | Switch Motor OFF    | Cooldown time ~20-60sec
 *                           |                      |                     |                     | Set Vh = 0, wait PWM stable (to avoid spark on switch)
 *                           |                      |                     |                     | Switch OFF heater
 *   ________________________|______________________|_____________________|_____________________|_________________
 *                           |                      |                     |                     |
 *    S- allowed             | prohibited           | Sv < Vm_off         | Th expired          | Heater can be disabled
 *                           | The very 1st state   | Heater Cooled       | St < Th_off         | unconditioanlly
 *                           |                      |                     |                     |
 *   ========================|======================|=====================|=====================|=================
 *                           |                      |                     |                     |
 *    error                  | Under/Over Temp1     | Under/Over Temp1    | Under/Over Temp1    | Under/Over Temp2
 *                           | Sv > Vv_closed_max   | Sv > Vm_off_max     |                     |
 *                           | (valve not closed)   | (motor still on)    |                     |
 *                           |                      |                     |                     |
 *   ========================|====================================================================================
 *                           |
 *    Alarm                  | Error for 1min
 *                           |
 *   =============================================================================================================
 *
 */

/*
 *   PWM values for heater control. Depends on payload.
 *   Non linear. Values tabified for 430Ohm resistor load
 *   Max HEATER_CTRL voltage = 10V
 *
 *  #   |    Voltage (V) | Pwm
 *  ----|----------------|--------
 *  0   |  0    0.1      | <15  // Value not used
 *  1   |  2    1.9      | 24
 *  2   |  4    4.1      | 32
 *  3   |  6    5.96     | 40
 *  4   |  8    7.97     | 52
 *  5   |  10   9.98     | 86
 *
 */
const uint8_t guca_heater_pwm_table[HAVC_HEATER_CTRL_VAL_NUM] = {  0,  24,  32, 40, 52, 86 };

hvac_t g_hvac;

int8_t g_ha_nlink_timer_cnt;
int16_t g_ha_hvac_fsm_timer_cnt = 0;
int16_t g_ha_hvac_fsm_timer = 0;

void hvac_on_rx(uint8_t idx, const uint8_t *buf_in)
{
    hvac_t *hvac = &g_hvac;
    UNREFERENCED_PARAM(idx);
    uint8_t len = buf_in[NLINK_HDR_OFF_LEN];
    UNREFERENCED_PARAM(len);

    if (buf_in[NLINK_HDR_OFF_TYPE] != NODE_TYPE_HVAC) {
        // Unexpected event type
        return;
    }

    hvac->state_trgt = (buf_in[NLINK_HDR_OFF_DATA + HVAC_DATA_STATE] & HVAC_DATA_STATE_TRGT_MASK) >> 4;
    hvac->heater_ctrl_mval = buf_in[NLINK_HDR_OFF_DATA + HVAC_DATA_HEATER_MVAL];

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

    // Set timer 0 to 8-bit Fast PWM, no prescaler, no cnt - free running
    TCCR0A = (1 << WGM00) | (1 << WGM01);
    TCCR0B = (1 << CS00);

    // Enable Timer0 overflow interrupt
    TIMSK = (1<<TOIE0);

    // NLINK configuration & init
    PCMSK |= NLINK_IO_RX_PIN_MASK;  // Enable Pin Change interrupt

    ha_i2c_init();

    ha_nlink_init();

    ha_hvac_init();

    g_ha_hvac_fsm_timer_cnt = 0;

    sei();

    // Endless loop
    while(1) {
        __asm__ __volatile__ ("    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"
                                ::);
        ha_nlink_check_rx();
        ha_nlink_check_tx();

        if (g_ha_hvac_fsm_timer) {
            g_ha_hvac_fsm_timer = 0;
            ha_hvac_fsm();
        }
    }
}

void hvac_heater_control(uint8_t pwm_idx)
{
    if (pwm_idx >= HAVC_HEATER_CTRL_VAL_NUM) pwm_idx = HAVC_HEATER_CTRL_VAL_NUM - 1;

    if (pwm_idx == 0) {
        // Disable PWM
        HVAC_HEATER_CONTROL_PORT &= ~HVAC_HEATER_CONTROL_MASK;
        TCCR0A &= ~((1 << COM0B1) | (1 << COM0B0));
    } else {
        uint8_t pwm_val = guca_heater_pwm_table[pwm_idx];
        TCCR0A |= (1 << COM0B1);
        OCR0B = pwm_val;
    }

}
void ha_hvac_node_update()
{
    node_t *node = g_hvac.node;
    hvac_t *hvac = &g_hvac;

    node->tx_buf[NLINK_HDR_OFF_TYPE] = NODE_TYPE_HVAC;
    node->tx_buf[NLINK_HDR_OFF_LEN] = HVAC_DATA_LAST;

    uint8_t val =
        (hvac->state_curr & HVAC_DATA_STATE_CURR_MASK) |
        (hvac->state_trgt & HVAC_DATA_STATE_TRGT_MASK);

    if (hvac->state_timer) val |= HVAC_DATA_STATE_STMR_MASK;
    if (0) val |= HVAC_DATA_STATE_ALRM_MASK;

    node->tx_buf[NLINK_HDR_OFF_DATA + HVAC_DATA_STATE ] = val;
    node->tx_buf[NLINK_HDR_OFF_DATA + HVAC_DATA_HEATER_MVAL] = hvac->heater_ctrl_mval;
    node->tx_buf[NLINK_HDR_OFF_DATA + HVAC_DATA_HEATER_CURR] = hvac->heater_ctrl_curr;

    // TODO: Fill Sensor data here
    // ...
}

void ha_hvac_init()
{
    hvac_t *hvac = &g_hvac;

    HVAC_HEATER_CONTROL_DIR |= HVAC_HEATER_CONTROL_MASK;   // output
    hvac_heater_control(0);

    HVAC_SWITCH_PORT &= ~HVAC_SWITCH_MASK;
    HVAC_SWITCH_DIR |= HVAC_SWITCH_MASK;

    HVAC_SWITCH_PORT |= HVAC_SWITCH_STATE_S1;

    hvac->state_trgt = HVAC_STATE_S1;
    hvac->state_curr = HVAC_STATE_S1;
    hvac->state_timer = HVAC_TIMER_VALVE_CLOSE; // Assume valve is in wrong state

    ha_phts_init();

    node_t *node = ha_nlink_node_register(HVAC_ADDR, NODE_TYPE_HVAC, hvac_on_rx);
    hvac->node = node;

}

/*
 * Set heater control voltage as function of temperature
 * sensor in case of auto regulation
 * or set it to fixed value in mode CONST
 */
static void hvac_heater_regulation(hvac_t *hvac) {

    if (hvac->heater_ctrl_mval & HVAC_HEATER_CTRL_MODE_MASK) {
        // TODO: Hc = func(St)
        hvac_heater_control(0);
    } else {
        uint8_t val = hvac->heater_ctrl_mval & HVAC_HEATER_CTRL_VAL_MASK;
        if (hvac->heater_ctrl_curr != val) {
            hvac_heater_control(val);
            hvac->heater_ctrl_curr = val;
        }
    }

}

static void wait_10ms() {
    // Wait 10ms in idle loop - NOT ISR!
    // 256 clocks @ 20 MHz ==> 12.8usec
    uint16_t cnt = ((20*1000)/24)*10; // 24 instruction in loop
                                    // TODO: ^^^ verify
    while(cnt--) {
        __asm__ __volatile__ ("    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"
                                ::);
    }

    return;
}


static void hvac_on_s_plus(){
    hvac_t *hvac = &g_hvac;

    // S+
    switch(hvac->state_curr) {
        case HVAC_STATE_S1:
            // Open valve unconditionally
            HVAC_SWITCH_PORT = HVAC_SWITCH_STATE_S2;
            hvac->state_curr = HVAC_STATE_S2;
            hvac->state_timer = HVAC_TIMER_VALVE_OPEN;
            break;
        case HVAC_STATE_S2:
            // Enable motor. Wait while valve is opened
            // Check Valve open timer (Tvo) expired
            if (hvac->state_timer > 0) {
                break;
            }
            HVAC_SWITCH_PORT = HVAC_SWITCH_STATE_S3_M1;
            hvac->state_curr = HVAC_STATE_S3;
            hvac->state_timer = HVAC_TIMER_MOTOR_START;
            break;
        case HVAC_STATE_S3:
            // Enable heater. Wait while motor run-up.
            if (hvac->state_timer > 0) {
                break;
            }
            hvac->state_curr = HVAC_STATE_S4;
            // Just in case
            // The heater will be controlled in runtime as
            // function of IN/OUT temperature or user setting
            hvac_heater_control(0);
            HVAC_SWITCH_PORT = HVAC_SWITCH_STATE_S4_M1;
            hvac->state_timer = HVAC_TIMER_MOTOR_START;
            break;
        case HVAC_STATE_S4:
            // prohibited
            break;
    }
}

static void hvac_on_s_minus(){
    hvac_t *hvac = &g_hvac;

    switch(hvac->state_curr) {
        case HVAC_STATE_S4:
            // Disable heater unconditionally.
            // Disable heater by its own control logic and wait
            // while it takes effect to avoid spark on switch
            hvac_heater_control(0);
            wait_10ms();
            HVAC_SWITCH_PORT = HVAC_SWITCH_STATE_S3_M1;
            hvac->state_curr = HVAC_STATE_S3;
            hvac->state_timer = HVAC_TIMER_HEATER_COOLDOWN; // Th timer
            break;
        case HVAC_STATE_S3:
            // Disable motor. Wait while heaters cools down. Th timer expired
            if (hvac->state_timer > 0) {
                break;
            }
            // Check outlet air temperature
            // if (outlet_air_still_very_hot)   // Heater switch malfunction

            HVAC_SWITCH_PORT = HVAC_SWITCH_STATE_S2;
            hvac->state_curr = HVAC_STATE_S2;
            break;
        case HVAC_STATE_S2:
            // Close valve.
            // Check airflow. Wait while motor is off. I.e. no airflow (Sv < Vm_off)
            // if (motor_still_runnig) ..       // Motor switch malfunction
            //
            hvac->state_curr = HVAC_STATE_S1;
            HVAC_SWITCH_PORT = HVAC_SWITCH_STATE_S1;
            hvac->state_timer = HVAC_TIMER_VALVE_CLOSE;
            break;
        case HVAC_STATE_S1:
            // prohibited
            break;
    }
}

static void hvac_check_error() {
    hvac_t *hvac = &g_hvac;

    switch(hvac->state_curr) {
        case HVAC_STATE_S1:
            // Under/Over Temp1
            // Sv > Vv_closed_max (valve not closed)
            break;
        case HVAC_STATE_S2:
            // Under/Over Temp1
            // Sv > Vm_off_max  (motor still on)
            break;
        case HVAC_STATE_S3:
            // Under/Over Temp1
            break;
        case HVAC_STATE_S4:
            // Under/Over Temp2
            break;
    }
}
/*
 * HVAC main logic executed @ 20ms
 */
void ha_hvac_fsm() {
    hvac_t *hvac = &g_hvac;

    ha_phts_poll(&hvac->sensor);

    hvac_heater_regulation(hvac);

    if (hvac->state_timer > 0) hvac->state_timer --;

    // Check transition
    if (hvac->state_curr < hvac->state_trgt) {
        hvac_on_s_plus();
    } else if (hvac->state_curr > hvac->state_trgt) {
        hvac_on_s_minus();
    }

    hvac_check_error();
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

    g_ha_hvac_fsm_timer_cnt ++;
    if (g_ha_hvac_fsm_timer_cnt > 80 * 20) {    // 20ms
        g_ha_hvac_fsm_timer_cnt = 0;
        g_ha_hvac_fsm_timer = 1;
    }
}
