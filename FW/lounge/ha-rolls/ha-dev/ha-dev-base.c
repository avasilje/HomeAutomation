// TODO: !!!!!!!!!!!!!!! Code review !!!!!!!!!!!!!!!
/*
* ha_dev_base.c
*
* Created: 3/30/2019 1:19:17 PM
*  Author: solit
*/
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/io.h>
#include "ha-common.h"
#include "ha-nlink.h"
#include "ha-dev-base.h"

int8_t g_ha_nlink_timer_cnt = -1;
#define SLOW_PWM_PERIOD 0xFF

// TODO: move to pgm?
// Note: Values are non linear so need to be calibrated for precise control
#define PWM_PULSE(duty_cycle)  ((duty_cycle * 256 / 100) - 1)
const uint8_t guca_pwm_intensity_table[INTENSITIES_NUM] EEMEM =
{
    0,               // 0   (OFF)
    PWM_PULSE(  3),  // 1
    PWM_PULSE(  6),  // 2
    PWM_PULSE( 10),  // 3
    PWM_PULSE( 14),  // 4
    PWM_PULSE( 18),  // 5
    PWM_PULSE( 23),  // 6
    PWM_PULSE( 28),  // 7
    PWM_PULSE( 33),  // 8
    PWM_PULSE( 38),  // 9
    PWM_PULSE( 44),  // 10
    PWM_PULSE( 50),  // 11
    PWM_PULSE( 56),  // 12
    PWM_PULSE( 63),  // 13
    PWM_PULSE( 70),  // 14
    PWM_PULSE( 75),  // 15
    PWM_PULSE( 85),  // 16
    PWM_PULSE( 93),  // 17
    0xFF             // 18 100% (ON)
};

void in_gpio_init() 
{
    // All SW configured as pull-up inputs
    IN_SW_0_PORT_CTRL = PORT_PULLUPEN_bm;
    IN_SW_1_PORT_CTRL = PORT_PULLUPEN_bm;
    IN_SW_2_PORT_CTRL = PORT_PULLUPEN_bm;
    IN_SW_3_PORT_CTRL = PORT_PULLUPEN_bm;
    IN_SW_4_PORT_CTRL = PORT_PULLUPEN_bm;
    IN_SW_5_PORT_CTRL = PORT_PULLUPEN_bm;
    IN_SW_6_PORT_CTRL = PORT_PULLUPEN_bm;

    IN_SW_0_PORT.DIRCLR = IN_SW_0_MASK;
    IN_SW_1_PORT.DIRCLR = IN_SW_1_MASK;
    IN_SW_2_PORT.DIRCLR = IN_SW_2_MASK;
    IN_SW_3_PORT.DIRCLR = IN_SW_3_MASK;
    IN_SW_4_PORT.DIRCLR = IN_SW_4_MASK;
    IN_SW_5_PORT.DIRCLR = IN_SW_5_MASK;
    IN_SW_6_PORT.DIRCLR = IN_SW_6_MASK;
}

void out_gpio_init()
{

    OUT_0_PORT.OUTCLR = OUT_0_MASK;
    OUT_1_PORT.OUTCLR = OUT_1_MASK;
    OUT_2_PORT.OUTCLR = OUT_2_MASK;
    OUT_3_PORT.OUTCLR = OUT_3_MASK;
    OUT_4_PORT.OUTCLR = OUT_4_MASK;
    OUT_5_PORT.OUTCLR = OUT_5_MASK;
    OUT_6_PORT.OUTCLR = OUT_6_MASK;

    OUT_0_PORT.DIRSET = OUT_0_MASK;
    OUT_1_PORT.DIRSET = OUT_1_MASK;
    OUT_2_PORT.DIRSET = OUT_2_MASK;
    OUT_3_PORT.DIRSET = OUT_3_MASK;
    OUT_4_PORT.DIRSET = OUT_4_MASK;
    OUT_5_PORT.DIRSET = OUT_5_MASK;
    OUT_6_PORT.DIRSET = OUT_6_MASK;

}

uint8_t ha_dev_base_get_in_pins() {

    uint8_t val = 0;
    if (IN_SW_0_VPIN & IN_SW_0_MASK) val |= 1 << 0;
    if (IN_SW_1_VPIN & IN_SW_1_MASK) val |= 1 << 1;
    if (IN_SW_2_VPIN & IN_SW_2_MASK) val |= 1 << 2;
    if (IN_SW_3_VPIN & IN_SW_3_MASK) val |= 1 << 3;
    if (IN_SW_4_VPIN & IN_SW_4_MASK) val |= 1 << 4;
    if (IN_SW_5_VPIN & IN_SW_5_MASK) val |= 1 << 5;
    if (IN_SW_6_VPIN & IN_SW_6_MASK) val |= 1 << 6;

    return val;
}

void fast_pwm_init() 
{
    // Period 20MHz / 256 = 12.8us = 78kHz

    // Initialize TCA in split mode
    // L0..2 => WO0..WO2 => C11, C9, C7
    // H0..2 => WO3..WO5 =>  C5, C3, C1

    TCA0.SPLIT.CTRLA &= ~TCA_SPLIT_ENABLE_bm;       // No prescaler.
    TCA0.SPLIT.CTRLESET = TCA_SPLIT_CMD_RESET_gc;

    TCA0.SPLIT.CTRLB =
        (TCA_SPLIT_LCMP0EN_bm |
         TCA_SPLIT_LCMP1EN_bm |
         TCA_SPLIT_LCMP2EN_bm |
         TCA_SPLIT_HCMP0EN_bm |
         TCA_SPLIT_HCMP1EN_bm |
         TCA_SPLIT_HCMP2EN_bm );

    TCA0.SPLIT.CTRLD = TCA_SPLIT_SPLITM_bm;

    TCA0.SPLIT.LPER = 0xFF;     // TODO: Q: or 0 ???
    TCA0.SPLIT.HPER = 0xFF;

    TCA0.SPLIT.CTRLA |= TCA_SPLIT_ENABLE_bm;

    TCA0.SPLIT.LCMP0 = 0x00;
    TCA0.SPLIT.LCMP1 = 0x00;
    TCA0.SPLIT.LCMP2 = 0x00;

    TCA0.SPLIT.HCMP0 = 0x00;
    TCA0.SPLIT.HCMP1 = 0x00;
    TCA0.SPLIT.HCMP2 = 0x00;

}

void slow_pwm_init()
{
    // Period 20MHz / 64 / 256 = 1.22kHz
    // ELG-150 series power supply - additive 10V PWM signal (frequency range 100Hz ~ 3KHz):

    // Initialize TCD in split mode
    // WOA => C3 PINA4
    // WOB => C1 PINA5

    //while (0 == (TCD0.STATUS & TCD_ENRDY_bm) );
    TCD0.CTRLA &= ~TCD_ENABLE_bm;

    // One Ramp Mode
    // Synchronization prescaler = ?
    // Counter prescaler = ?
    TCD0.CTRLA = 
        TCD_CLKSEL_SYSCLK_gc | 
        TCD_SYNCPRES_DIV2_gc |
        TCD_CNTPRES_DIV32_gc;

    TCD0.CTRLB = TCD_WGMODE_ONERAMP_gc;
    TCD0.CTRLC = TCD_AUPDATE_bm;            // Synchronized on CMPBCLR write
    // TCD0.CTRLC = CMPOVR;

    // TCD0.CTRLC = CMPOVR;
    // TCD0.CTRLD = 0x01    | (0x01 << 6)
    //               ^^^A       ^^^B


    CPU_CCP = CCP_IOREG_gc;
    TCD0.FAULTCTRL = (
//        TCD_CMPAEN_bm |
        TCD_CMPBEN_bm |
        0);

    while (0 == (TCD0.STATUS & TCD_ENRDY_bm) );
    TCD0.CTRLA |= TCD_ENABLE_bm;

    TCD0.CMPASET = 0;
    
    TCD0.CMPACLR = 0;                   // Min value at reset   // C1, WOB, PINA5
    TCD0.CMPBSET = SLOW_PWM_PERIOD;     // Min value at reset   // C3, WOA, PINA4

    TCD0.CMPBCLR = SLOW_PWM_PERIOD; // Period. Must be written last

//   while (0 == (TCD0.STATUS & TCD_CMDRDY_bm) );   // Syncronize before command execution
    

}

void timer_init()
{
    TCB0.CCMP = 0x0080;
    TCB0.INTFLAGS = TCB_CAPT_bm;                    // Clear CAPTURE interrupt
    TCB0.INTCTRL = TCB_CAPT_bm;                     // CAPT set when CNT = CCMPH
    TCB0.CTRLB = TCB_CNTMODE_PWM8_gc;
    TCB0.CTRLA = TCB_ENABLE_bm | TCB_CLKSEL0_bm;    // 1/2 Prescaler, CLK_PER as input
}

void ha_nlink_gpio_init()
{
    NLINK_IO_RX_PORT |= NLINK_IO_RX_PIN_MASK;
    NLINK_IO_RX_DIR  &= ~NLINK_IO_RX_PIN_MASK;
    PORTC.PIN5CTRL = PORT_ISC_FALLING_gc | PORT_PULLUPEN_bm;

    NLINK_IO_TX_PORT &= ~NLINK_IO_TX_PIN_MASK;
    NLINK_IO_TX_DIR  |= NLINK_IO_TX_PIN_MASK;   // output
    PORTC.PIN4CTRL = PORT_ISC_INPUT_DISABLE_gc;

    NLINK_IO_DBG_PORT &= ~(NLINK_IO_DBG_PIN0_MASK | NLINK_IO_DBG_PIN1_MASK);
    NLINK_IO_DBG_DIR |= (NLINK_IO_DBG_PIN0_MASK | NLINK_IO_DBG_PIN1_MASK);
}

void ha_dev_base_init()
{
    //    CPUINT.CTRLA = 0;     // Reset
    //    CPUINT.STATUS = 0;
    //    CPUINT.LVL0PRI = 0;
    //    CPUINT.LVL1VEC = 0;

    // FUSE.OSCCFG = 2 = default => 20MHz
    CPU_CCP = CCP_IOREG_gc;
    CLKCTRL.MCLKCTRLB = 0;   // No prescaler. CLK_PER = 20MHz

    timer_init();
    in_gpio_init();
    out_gpio_init();
    fast_pwm_init();
    //slow_pwm_init();
}


void ha_dev_base_set_steady(uint8_t mask, uint8_t val)
{
    // Val 0 ==> On ==> output 1
    // Val 1 ==> Off ==> output 0

    // L0..2 => WO0..WO2 => C11, C9, C7 => PB0, PB1, PB2
    // H0..2 => WO3..WO5 =>  C5, C3, C1 => PA3, PA4, PA5

    switch(mask) {
        case 0x01:
            // WO5, C1, PINA5
            TCA0.SPLIT.CTRLB &= ~TCA_SPLIT_HCMP2EN_bm;
            if (val) {
                OUT_0_PORT.OUTSET = OUT_0_MASK;
            } else {
                OUT_0_PORT.OUTCLR = OUT_0_MASK;
            }
            break;

        case 0x02:
            TCA0.SPLIT.CTRLB &= ~TCA_SPLIT_HCMP1EN_bm;
            if (val) {
                OUT_1_PORT.OUTSET = OUT_1_MASK;
            } else {
                OUT_1_PORT.OUTCLR = OUT_1_MASK;
            }
            break;

        case 0x04:
            TCA0.SPLIT.CTRLB &= ~TCA_SPLIT_HCMP0EN_bm;
            if (val) {
                OUT_2_PORT.OUTSET = OUT_2_MASK;
             } else {
                OUT_2_PORT.OUTCLR = OUT_2_MASK;
            }
            break;
        case 0x08:
            TCA0.SPLIT.CTRLB &= ~TCA_SPLIT_LCMP2EN_bm;
            if (val) {
                OUT_3_PORT.OUTSET = OUT_3_MASK;
            } else {
                OUT_3_PORT.OUTCLR = OUT_3_MASK;
            }
            break;
        case 0x10:
            TCA0.SPLIT.CTRLB &= ~TCA_SPLIT_LCMP1EN_bm;
            if (val) {
                OUT_4_PORT.OUTSET = OUT_4_MASK;
            } else {
                OUT_4_PORT.OUTCLR = OUT_4_MASK;
            }
            break;
        case 0x20:
            TCA0.SPLIT.CTRLB &= ~TCA_SPLIT_LCMP0EN_bm;
            if (val) {
                OUT_5_PORT.OUTSET = OUT_5_MASK;
            } else {
                OUT_5_PORT.OUTCLR = OUT_5_MASK;
            }
            break;
        default:
            break;
    }

    return;
}

void ha_dev_base_set_slow_pwm (uint8_t ch_num, uint8_t pwm_val_idx)
{
    uint8_t pwm_val = guca_pwm_intensity_table[pwm_val_idx];
    
    // Select PWM register
    switch(ch_num) {
        case 0: // C1, WOB, PINA5
            TCD0.CMPBSET = SLOW_PWM_PERIOD - pwm_val;
            TCD0.CMPBCLR = SLOW_PWM_PERIOD;
            break;
        case 1: // C3, WOA, PINA4
            TCD0.CMPACLR = pwm_val;
            TCD0.CMPBCLR = SLOW_PWM_PERIOD;
            break;
    }
}

void ha_dev_base_set_fast_pwm (uint8_t mask, uint8_t pwm_val_idx)
{
    uint8_t pwm_val = guca_pwm_intensity_table[pwm_val_idx];

    // Select PWM register
    // L0..2 => WO0..WO2 => C11, C9, C7 => PB0, PB1, PB2
    // H0..2 => WO3..WO5 =>  C5, C3, C1 => PA3, PA4, PA5

    switch(mask) {
        case 0x01: // C1
            TCA0.SPLIT.HCMP2 = pwm_val;
            TCA0.SPLIT.CTRLB |= TCA_SPLIT_HCMP2EN_bm;
            break;
        case 0x02: // C3
            TCA0.SPLIT.HCMP1 = pwm_val;
            TCA0.SPLIT.CTRLB |= TCA_SPLIT_HCMP1EN_bm;
            break;
        case 0x04: // C5
            TCA0.SPLIT.HCMP0 = pwm_val;
            TCA0.SPLIT.CTRLB |= TCA_SPLIT_HCMP0EN_bm;
            break;
        case 0x08: // C7
            TCA0.SPLIT.LCMP2 = pwm_val;
            TCA0.SPLIT.CTRLB |= TCA_SPLIT_LCMP2EN_bm;
            break;
        case 0x10: // C9
            TCA0.SPLIT.LCMP1 = pwm_val;
            TCA0.SPLIT.CTRLB |= TCA_SPLIT_LCMP1EN_bm;
            break;
        case 0x20: // C11
            TCA0.SPLIT.LCMP0 = pwm_val;
            TCA0.SPLIT.CTRLB |= TCA_SPLIT_LCMP0EN_bm;
            break;
    }
}

ISR(PORTC_PORT_vect) {
    if (NLINK_IO_RX_INTFLAG & NLINK_IO_RX_PIN_MASK) {
        NLINK_IO_RX_INTFLAG = NLINK_IO_RX_PIN_MASK;
        // Call NLINK start callback on RX pin falling edge
        isr_nlink_io_on_start_edge();
    }
}

// Interrupt triggered every 256 timer clocks and count periods
ISR(TCB0_INT_vect) {

    // 256 clocks @ 20 MHz ==> 12.8usec
    TCB0.INTFLAGS = TCB_CAPT_bm;     // Clear CAPTURE interrupt

    // Call NLINK timer callback every 256usec
    if (g_ha_nlink_timer_cnt >= 0) {
        g_ha_nlink_timer_cnt++;
        if (g_ha_nlink_timer_cnt == 20) {
            g_ha_nlink_timer_cnt = 0;
            isr_nlink_io_on_timer();
        }
    }

    isr_ha_app_on_timer();

}

