/*
 * ha-blcn-config.c
 *
 * Created: 28/12/2021
 *  Author: solit
 */
#include <avr/eeprom.h>

#include "ha-common.h"
#include "ha-nlink.h"
#include "ha-node-switch.h"
#include "ha-node-ledlight.h"
#include "ha-node-roll.h"

#define DIMM_ON_INTENSITIES_NUM 11
const uint8_t guca_dimm_on_intensity_table[DIMM_ON_INTENSITIES_NUM] EEMEM =
{
    1,                  // MIN
    6,
    11,
    15,
    INTENSITIES_NUM-1   // MAX
};

#define DISABLE_ROLL_ON_MASK_NUM 5
const uint8_t guca_disable_roll_table[DISABLE_ROLL_ON_MASK_NUM] EEMEM =
{
    0,                  // 0000
    13,                 // 1101
    11,                 // 1011
    7,                  // 0111
    14,                 // 1110
};

const ha_node_sw_cfg_t blcn_sw_cfg EEMEM = {
    .switches_num = 5,
    .node_addr = SWITCH_ADDR_BLCN,
};

// LED2 (0x04 - C5) - Balcony light
#define BLCN_LEDS_CHANNELS_NUM 1
const uint8_t blcn_leds_ch_mask[BLCN_LEDS_CHANNELS_NUM] EEMEM = {0x01};

const ha_node_ll_cfg_t blcn_ll_cfg = {
    .node_addr = LEDLIGHT_ADDR_BLCN,

    .fadein_period = 1,
    .fadeout_period = 1,

    .leds_num = BLCN_LEDS_CHANNELS_NUM,
    .leds_ch_masks = blcn_leds_ch_mask,

    .dimms_num = DIMM_ON_INTENSITIES_NUM,
    .dimms = guca_dimm_on_intensity_table,

    .disable_masks_num = DISABLE_ROLL_ON_MASK_NUM,
    .disable_masks = guca_disable_roll_table
};

#define BLCN_SW_LED           0
#define BLCN_SW_ROLL_D_UP     1   // EXT-C4,  DOOR ROLL UP   - SW_IDX: 0x01
#define BLCN_SW_ROLL_D_DOWN   2   // EXT-C6,  DOOR ROLL DOWN - SW_IDX: 0x02
#define BLCN_SW_ROLL_W_UP     3   // EXT-C8,  WND  ROLL UP   - SW_IDX: 0x03
#define BLCN_SW_ROLL_W_DOWN   4   // EXT-C10, WND  ROLL DOWN - SW_IDX: 0x04

const ha_node_ll_evt_action_t blcn_ll_action[] = {

    // .---------------------------------------------------------------------------------------------------------------.
    // |                                   Event from                                   |    Led Action                |
    // |--------------------------------------------------------------------------------|------------------------------|
    // |   Type          |     Addr          | Param (SW idx, EVT type)                 |    Type            |  Param  |
    // |-----------------|-------------------|------------------------------------------|--------------------|---------|
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_BLCN,  (BLCN_SW_LED << 4) | SW_EVENT_ON_OFF,       LL_ACT_TYPE_TOGGLE,   0     },
    HA_NODE_LL_EVT_ACTION_EOT
};

/*******************  ROLL Door *******************/
#define BLCN_ROLLS_D_CHANNELS_NUM 1
const roll_ch_mask_t blcn_roll_d_ch_mask[BLCN_ROLLS_D_CHANNELS_NUM] EEMEM = {
     {.up = 0x02, .down = 0x04},
};

const  ha_node_roll_cfg_t blcn_roll_d_cfg = {
    .node_addr = ROLL_D_ADDR_BLCN,
    .rolls_num = BLCN_ROLLS_D_CHANNELS_NUM,
    .ee_ch_mask = blcn_roll_d_ch_mask,
    .active_timeout = 1000,  // in 10ms ticks
};

const ha_node_roll_evt_action_t blcn_roll_d_action[] = {

    // .--------------------------------------------------------------------------------------------------------------.
    // |                                   Event from                                     |    Led Action             |
    // |----------------------------------------------------------------------------------|---------------------------|
    // |   Type          |     Addr          | Param (SW idx, EVT type)                   |    Type         |  Param  |
    // |-----------------|-------------------|--------------------------------------------|-----------------|---------|
    {   NODE_TYPE_SWITCH,  SWITCH_ADDR_BLCN,  (BLCN_SW_ROLL_D_UP   << 4) | SW_EVENT_ON_OFF,   ROLL_ACT_TYPE_UP,     0  },
    {   NODE_TYPE_SWITCH,  SWITCH_ADDR_BLCN,  (BLCN_SW_ROLL_D_UP   << 4) | SW_EVENT_ON_HOLD,  ROLL_ACT_TYPE_UP,     0  },
    {   NODE_TYPE_SWITCH,  SWITCH_ADDR_BLCN,  (BLCN_SW_ROLL_D_UP   << 4) | SW_EVENT_HOLD_OFF, ROLL_ACT_TYPE_STOP,   0  },
    {   NODE_TYPE_SWITCH,  SWITCH_ADDR_BLCN,  (BLCN_SW_ROLL_D_DOWN << 4) | SW_EVENT_ON_OFF,   ROLL_ACT_TYPE_UP,     0  },
    {   NODE_TYPE_SWITCH,  SWITCH_ADDR_BLCN,  (BLCN_SW_ROLL_D_DOWN << 4) | SW_EVENT_ON_HOLD,  ROLL_ACT_TYPE_UP,     0  },
    {   NODE_TYPE_SWITCH,  SWITCH_ADDR_BLCN,  (BLCN_SW_ROLL_D_DOWN << 4) | SW_EVENT_HOLD_OFF, ROLL_ACT_TYPE_STOP,   0  },
    HA_NODE_ROLL_EVT_ACTION_EOT
};


/*******************  ROLL Window *******************/

#define BLCN_ROLLS_W_CHANNELS_NUM 1
const roll_ch_mask_t blcn_roll_w_ch_mask[BLCN_ROLLS_W_CHANNELS_NUM] EEMEM = {
    {.up = 0x08, .down = 0x10},
};

const  ha_node_roll_cfg_t blcn_roll_w_cfg = {
    .node_addr = ROLL_W_ADDR_BLCN,
    .rolls_num = BLCN_ROLLS_W_CHANNELS_NUM,
    .ee_ch_mask = blcn_roll_w_ch_mask,
    .active_timeout = 1000,  // in 10ms ticks
};

const ha_node_roll_evt_action_t blcn_roll_w_action[] = {

    // .--------------------------------------------------------------------------------------------------------------.
    // |                                   Event from                                     |    Led Action             |
    // |----------------------------------------------------------------------------------|---------------------------|
    // |   Type          |     Addr          | Param (SW idx, EVT type)                   |    Type         |  Param  |
    // |-----------------|-------------------|--------------------------------------------|-----------------|---------|
    {   NODE_TYPE_SWITCH,  SWITCH_ADDR_BLCN,  (BLCN_SW_ROLL_W_UP   << 4) | SW_EVENT_ON_OFF,   ROLL_ACT_TYPE_UP,     0  },
    {   NODE_TYPE_SWITCH,  SWITCH_ADDR_BLCN,  (BLCN_SW_ROLL_W_UP   << 4) | SW_EVENT_ON_HOLD,  ROLL_ACT_TYPE_UP,     0  },
    {   NODE_TYPE_SWITCH,  SWITCH_ADDR_BLCN,  (BLCN_SW_ROLL_W_UP   << 4) | SW_EVENT_HOLD_OFF, ROLL_ACT_TYPE_STOP,   0  },
    {   NODE_TYPE_SWITCH,  SWITCH_ADDR_BLCN,  (BLCN_SW_ROLL_W_DOWN << 4) | SW_EVENT_ON_OFF,   ROLL_ACT_TYPE_UP,     0  },
    {   NODE_TYPE_SWITCH,  SWITCH_ADDR_BLCN,  (BLCN_SW_ROLL_W_DOWN << 4) | SW_EVENT_ON_HOLD,  ROLL_ACT_TYPE_UP,     0  },
    {   NODE_TYPE_SWITCH,  SWITCH_ADDR_BLCN,  (BLCN_SW_ROLL_W_DOWN << 4) | SW_EVENT_HOLD_OFF, ROLL_ACT_TYPE_STOP,   0  },
    HA_NODE_ROLL_EVT_ACTION_EOT
};
