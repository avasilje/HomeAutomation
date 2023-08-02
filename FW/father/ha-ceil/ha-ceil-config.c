/*
 * ha-ceil-config.c
 *
 * Created: 4/28/2019 5:23:24 AM
 *  Author: solit
 */ 
 #include <avr/eeprom.h>

 #include "ha-common.h"
 #include "ha-nlink.h"
 #include "ha-node-switch.h"
 #include "ha-node-ledlight.h"

 #include "ha-mainlight.h"
 #include "ha-mainlight-const.h"

/* Shared with AMBILED/CEIL */
#define SWITCH_ADDR_AMBILED (SWITCH_ADDR+1)
#define SWITCH_ADDR_CEIL (SWITCH_ADDR+0)

const ha_node_sw_cfg_t ceil_sw_cfg EEMEM = {
    .switches_num = 5,
	.node_addr = SWITCH_ADDR_CEIL,
};

// --- in use ---
// LED0 (0x01  - C1) - Room ambient     (AMB-1, AMB-2)
// LED1 (0x02 - C3) - Hall spot        (SPT-K)
// LED2 (0x04 - C5) - Room spot mid    (SPT-3, SPT-4)
// LED3 (0x10 - C9) - Room spot bed    (SPT-1, SPT-2)
// --- unused ---
// LED4 (0x08 - C7) - Room spot unused (SPT-5, SPT-6) (doesn't work :[ )

#define ROOM_AMB_LEDS_CHANNELS_NUM 1
const uint8_t room_amb_leds_ch_mask[ROOM_AMB_LEDS_CHANNELS_NUM] EEMEM = {0x01};

#define HALL_SPT_LEDS_CHANNELS_NUM 1
const uint8_t hall_spt_leds_ch_mask[HALL_SPT_LEDS_CHANNELS_NUM] EEMEM = {0x02};

#define ROOM_SPT_MID_LEDS_CHANNELS_NUM 1
const uint8_t room_spt_mid_leds_ch_mask[ROOM_SPT_MID_LEDS_CHANNELS_NUM] EEMEM = {0x04};

#define ROOM_SPT_BED_LEDS_CHANNELS_NUM 1
const uint8_t room_spt_bed_leds_ch_mask[ROOM_SPT_BED_LEDS_CHANNELS_NUM] EEMEM = {0x10};

const ha_node_ll_cfg_t room_amb_ll_cfg = {
    .node_addr = LEDLIGHT_ADDR + 0,

    .fadein_period = 1,
    .fadeout_period = 1,

    .leds_num = ROOM_AMB_LEDS_CHANNELS_NUM,
    .leds_ch_masks = room_amb_leds_ch_mask,

    .dimms_num = DIMM_ON_INTENSITIES_NUM,
    .dimms = guca_dimm_on_intensity_table,

	.disable_masks_num = 0,
	.disable_masks = NULL
};

const ha_node_ll_cfg_t hall_spt_ll_cfg = {
    .node_addr = LEDLIGHT_ADDR + 1,

    .fadein_period = 1,
    .fadeout_period = 1,

    .leds_num = HALL_SPT_LEDS_CHANNELS_NUM,
    .leds_ch_masks = hall_spt_leds_ch_mask,

    .dimms_num = DIMM_ON_INTENSITIES_NUM,
    .dimms = guca_dimm_on_intensity_table,

	.disable_masks_num = 0,
	.disable_masks = NULL
};

const ha_node_ll_cfg_t room_spt_mid_ll_cfg = {
	.node_addr = LEDLIGHT_ADDR + 2,

	.fadein_period = 1,
	.fadeout_period = 1,

	.leds_num = ROOM_SPT_MID_LEDS_CHANNELS_NUM,
	.leds_ch_masks = room_spt_mid_leds_ch_mask,

	.dimms_num = DIMM_ON_INTENSITIES_NUM,
	.dimms = guca_dimm_on_intensity_table,

	.disable_masks_num = 0,
	.disable_masks = NULL
};

const ha_node_ll_cfg_t room_spt_bed_ll_cfg = {
    .node_addr = LEDLIGHT_ADDR + 3,

    .fadein_period = 1,
    .fadeout_period = 1,

    .leds_num = ROOM_SPT_BED_LEDS_CHANNELS_NUM,
    .leds_ch_masks = room_spt_bed_leds_ch_mask,

    .dimms_num = DIMM_ON_INTENSITIES_NUM,
    .dimms = guca_dimm_on_intensity_table,

	.disable_masks_num = 0,
	.disable_masks = NULL
};

    // The led light use the table to react on external events.
    // The table created by the integration and passed as a param to the corresponding ledlight node
    // during a configuration.


    // All High switches toggle spot light
    // All Low switches toggle room central light
    
    // LED0 - Room ambient (AMB-1, AMB-2)
    // LED1 - Hall spot  (SPT-K)
    // LED2 - Room spot mid (SPT-3, SPT-4)
    // LED3 - Room spot bed (SPT-1, SPT-2)
    // LED4 - Room spot xxx (SPT-5, SPT-6) - not in use

    // EXT-C2, Plan:SW-4 - Wall H - SW_IDX: 0x00 - Toggle spot-mid (SPT-3, SPT-4 - LED2)
    // EXT-C4, Plan:SW-7 - Hall H - SW_IDX: 0x01 - Toggle hall spots (SPT-K - LED1)
    // EXT-C6, Plan:SW-5 - Wall L - SW_IDX: 0x02 - Toggle room ambient (SPT-AMB1, SPT-AMB2, LED0)
    // EXT-C8  Plan:SW-1 - BED  L - SW_IDX: 0x03 - Toggle spot-bed (SPT-1, SPT-2)
    // EXT-C10 Plan:SW-6 - Hall L - SW_IDX: 0x04 - Toggle room ambient

#define CEIL_SW_WALL_H 0
#define CEIL_SW_HALL_H 1
#define CEIL_SW_WALL_L 2
#define CEIL_SW_BED_L  3
#define CEIL_SW_HALL_L 4

const ha_node_ll_evt_action_t room_amb_ll_action[] = {

    // TODO: Split table into several LL nodes config
    // .---------------------------------------------------------------------------------------------------------------.
    // |                                   Event from                                   |    Led Action                |
    // |--------------------------------------------------------------------------------|------------------------------|
    // |   Type          |     Addr          | Param (SW idx, EVT type)                 |    Type            |  Param  |
    // |-----------------|-------------------|------------------------------------------|--------------------|---------|
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_WALL_L << 4) | SW_EVENT_ON_OFF,  LL_ACT_TYPE_TOGGLE,   0      },  // SCH_SW2 C6,  SW5 (WALL_L) release -> Room Ambient toggle
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_HALL_L << 4) | SW_EVENT_ON_OFF,  LL_ACT_TYPE_TOGGLE,   0      },  // SCH_SW4 C10, SW7 (HALL_L) release -> Room Ambient toggle
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_WALL_L << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // SCH_SW2 C6,  SW5 (WALL_L) hold    -> ALL OFF
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_HALL_L << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // SCH_SW4 C10, SW7 (HALL_L) hold    -> ALL OFF
	{   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_BED_L  << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // SCH_SW3 C8,  SW1 (BED_L)  hold    -> ALL OFF
    HA_NODE_LL_EVT_ACTION_EOT
};

const ha_node_ll_evt_action_t hall_spt_ll_action[] = {
    // .---------------------------------------------------------------------------------------------------------------.
    // |                                 Event from                                     |    Led Action                |
    // |--------------------------------------------------------------------------------|------------------------------|
    // |   Type          |     Addr          | Param (SW idx, EVT type)                 |    Type            |  Param  |
    // |-----------------|-------------------|------------------------------------------|--------------------|---------|
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_HALL_H << 4) | SW_EVENT_ON_OFF,  LL_ACT_TYPE_TOGGLE,   0      },  // SCH_SW1 C4,  SW6 (HALL_H) release -> Hall Spots toggle
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_HALL_H << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_DIMM,     0      },  // SCH_SW1 C4,  SW6 (HALL_H) release -> Hall Spots dimming
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_WALL_L << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // SCH_SW2 C6,  SW5 (WALL_L) hold    -> ALL OFF
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_HALL_L << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // SCH_SW4 C10, SW7 (HALL_L) hold    -> ALL OFF
	{   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_BED_L  << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // SCH_SW3 C8,  SW1 (BED_L)  hold    -> ALL OFF
    HA_NODE_LL_EVT_ACTION_EOT
};

const ha_node_ll_evt_action_t room_spt_mid_ll_action[] = {
    // .---------------------------------------------------------------------------------------------------------------.
    // |                                 Event from                                     |    Led Action                |
    // |--------------------------------------------------------------------------------|------------------------------|
    // |   Type          |     Addr           | Param (SW num, EVT type)                |    Type            |  Param  |
    // |-----------------|--------------------|-----------------------------------------|--------------------|---------|
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_WALL_H << 4) | SW_EVENT_ON_OFF,  LL_ACT_TYPE_TOGGLE,   0      },  // SCH_SW0 C2,  SW4 (WALL_H) release -> Room Spots Mid toggle
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_WALL_H << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_DIMM,     0      },  // SCH_SW0 C2,  SW4 (WALL_H) hold    -> Room Spots Mid dimming
    {   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_WALL_L << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // SCH_SW2 C6,  SW5 (WALL_L) hold    -> ALL OFF
	{   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_HALL_L << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // SCH_SW4 C10, SW7 (HALL_L) hold    -> ALL OFF
	{   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_BED_L  << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // SCH_SW3 C8,  SW1 (BED_L)  hold    -> ALL OFF
    HA_NODE_LL_EVT_ACTION_EOT
};

const ha_node_ll_evt_action_t room_spt_bed_ll_action[] = {
	// .-----------------------------------------------------------------------------------------------------------.
	// |                        Event from                                          |    Led Action                |
	// |----------------------------------------------------------------------------|------------------------------|
	// |   Type          |     Addr            | Param (SW num, EVT type)           |    Type            |  Param  |
	// |-----------------|---------------------|------------------------------------|--------------------|---------|
	{   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_BED_L  << 4) | SW_EVENT_ON_OFF,  LL_ACT_TYPE_TOGGLE,   0      },  // SCH_SW3 C8,  SW1 (BED_L) release -> Room Spots Bed toggle
	{   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_BED_L  << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // SCH_SW3 C8,  SW1 (BED_L)  hold   -> ALL OFF
	{   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_WALL_L << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // SCH_SW2 C6,  SW5 (WALL_L) hold   -> ALL OFF
	{   NODE_TYPE_SWITCH,   SWITCH_ADDR_CEIL,  (CEIL_SW_HALL_L << 4) | SW_EVENT_ON_HOLD, LL_ACT_TYPE_OFF,      0      },  // SCH_SW4 C10, SW7 (HALL_L) hold   -> ALL OFF
	HA_NODE_LL_EVT_ACTION_EOT
};
