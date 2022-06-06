/*
 * ha-ambiled-const.c
 *
 * Created: 11/12/2021
 *  Author: solit
 */
#include <stdint.h>
#include <avr/eeprom.h>

#include "ha-common.h"
#include "ha-ambiled-const.h"

const uint8_t guca_dimm_on_intensity_table[DIMM_ON_INTENSITIES_NUM] EEMEM =
{
    7,                  // MIN
    9,
    11,
    13,
    15,
    INTENSITIES_NUM-1   // MAX
};

const uint8_t guca_disable_roll_table[DISABLE_ROLL_ON_MASK_NUM] EEMEM =
{
    0,                  // 0000
    13,                 // 1101
    11,                 // 1011
    7,                  // 0111
    14,                 // 1110
};
