/*
 * ha_ledlight_const.c
 *
 * Created: 5/6/2018 2:21:54 AM
 *  Author: solit
 */
#include <stdint.h>
#include "ha-common.h"
#include "ha-ledlight_const.h"

// TODO: move to pgm?
// Note: Values are non linear so need to be calibrated for precise control
#define PWM_PULSE(duty_cycle)  ((duty_cycle * 256 / 100) - 1)
const uint8_t guca_pwm_intensity_table[INTENSITIES_NUM] =
{
  PWM_PULSE(  0),  // 0   (OFF) // PWM not used
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
  PWM_PULSE(100)   // 18 100% (ON)  // PWM not used
};

const uint8_t guca_dimm_on_intensity_table[DIMM_ON_INTENSITIES_NUM] =
{
    1,                  // MIN
    6,
    11,
    15,
    INTENSITIES_NUM-1   // MAX
};
#if 0
const uint8_t guca_disable_roll_table[] =
{
    0,                  // 0000
    1,                  // 0001
    2,                  // 0010
    4,                  // 0100
    8,                  // 1000
    3,                  // 0011
    6,                  // 0110
    12,                 // 1100
    9,                  // 1001
    10,                 // 1010
    5,                  // 0101
    13,                 // 1101
    11,                 // 1011
    7,                  // 0111
    14,                 // 1110
    15,                 // 1111
};
#endif

const uint8_t guca_disable_roll_table[ROLL_ON_MASK_NUM] =
{
    0,                  // 0000
    13,                 // 1101
    11,                 // 1011
    7,                  // 0111
    14,                 // 1110
};
