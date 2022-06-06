/*
 * ha_ledlight_const.c
 *
 * Created: 5/6/2018 2:21:54 AM
 *  Author: solit
 */
#include <stdint.h>
#include "ha-common.h"
#include "ha-mainlight-const.h"

const uint8_t guca_dimm_on_intensity_table[DIMM_ON_INTENSITIES_NUM] EEMEM =
{
    1,                  // MIN
    6,
    11,
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
