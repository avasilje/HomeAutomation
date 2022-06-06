/*
 * ha_ceil1_config.c
 *
 * Created: 4/28/2019 5:23:24 AM
 *  Author: solit
 */ 
#include <avr/eeprom.h>

 #include "ha-common.h"
 #include "ha-nlink.h"
 #include "ha-node-switch.h"
 #include "ha-node-ledlight.h"

 #include "ha-ambiled.h"
 #include "ha-ambiled-const.h"

/* Shared with AMBILED/CEIL */
#define SWITCH_ADDR_AMBILED (SWITCH_ADDR+1)
#define SWITCH_ADDR_CEIL    (SWITCH_ADDR+0)

#define AMBILED_LEDS_CHANNELS_NUM 4
const uint8_t ambiled_leds_ch_mask[AMBILED_LEDS_CHANNELS_NUM] EEMEM = {0x01, 0x02, 0x04, 0x08};

const ha_node_sw_cfg_t ambiled_sw_cfg EEMEM = {
    .switches_num = 1,
    .node_addr = SWITCH_ADDR_AMBILED,
};

const ha_node_ll_cfg_t ambiled_ll_cfg = {
    .node_addr = LEDLIGHT_ADDR + 5,

    .fadein_period = 1,
    .fadeout_period = 1,

    .leds_num = AMBILED_LEDS_CHANNELS_NUM,
    .leds_ch_masks = ambiled_leds_ch_mask,

    .dimms_num = DIMM_ON_INTENSITIES_NUM,
    .dimms = guca_dimm_on_intensity_table,

    .disable_masks_num = DISABLE_ROLL_ON_MASK_NUM,
    .disable_masks = guca_disable_roll_table
};

#define AMBI_SW_BED_H 0

/* Part of CEIL project */
#define CEIL_SW_WALL_H 0
#define CEIL_SW_HALL_H 1
#define CEIL_SW_WALL_L 2
#define CEIL_SW_BED_L  3
#define CEIL_SW_HALL_L 4

const ha_node_ll_evt_action_t ambiled_ll_action[] = {

    // .-----------------------------------------------------------------------------------------------------------------.
    // |                                            Event from                            |    Led Action                |
    // |----------------------------------------------------------------------------------|------------------------------|
    // |   Type          |     Node address      | Param (SW idx, EVT type)               |    Type            |  Param  |
    // |-----------------|-----------------------|----------------------------------------|--------------------|---------|
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_AMBILED,  (AMBI_SW_BED_H  << 4) | SW_EVENT_ON_OFF,  LL_ACT_TYPE_TOGGLE,   0      },  // (BED_H) release -> Room Ambient toggle
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_AMBILED,  (AMBI_SW_BED_H  << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_ROLLDIMM, 0      },  // (BED_H) release -> Dimm or Roll depends on state
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,     (CEIL_SW_WALL_L << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // (SW_WALL_L) hold -> ALL OFF
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,     (CEIL_SW_HALL_L << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // (SW_HALL_L) hold -> ALL OFF
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,     (CEIL_SW_BED_L  << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // (SW_BED_L)  hold -> ALL OFF
    HA_NODE_LL_EVT_ACTION_EOT
};
