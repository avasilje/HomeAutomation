#pragma once
#include <avr/eeprom.h>
/*
 * ha_ledlight_const.h
 *
 * Created: 5/6/2018 2:24:38 AM
 *  Author: solit
 */

extern const uint8_t guca_pwm_intensity_table[INTENSITIES_NUM] EEMEM;

#define DIMM_ON_INTENSITIES_NUM 11
extern const uint8_t guca_dimm_on_intensity_table[DIMM_ON_INTENSITIES_NUM] EEMEM;

#define DISABLE_ROLL_ON_MASK_NUM 5
extern const uint8_t guca_disable_roll_table[DISABLE_ROLL_ON_MASK_NUM] EEMEM;

#define LED_FADEIN_PERIOD     1     // in 10ms ticks
#define LED_FADEOUT_PERIOD    1     // in 10ms ticks
