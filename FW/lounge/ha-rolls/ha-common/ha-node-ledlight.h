/*
 * ha_node_ledlight.h
 *
 * Created: 4/30/2018 4:37:47 PM
 *  Author: Solit
 */
#pragma once

typedef struct ha_node_ll_cfg_s {
    uint8_t node_addr;

    uint8_t fadein_period;         // In parent defined ticks
    uint8_t fadeout_period;        // In parent defined ticks

    uint8_t leds_num;
    HA_EEMEM const uint8_t *leds_ch_masks;

    uint8_t dimms_num;             // Number of entries in the array
    HA_EEMEM const uint8_t *dimms; // Array of intensities (0..max) to roll over while light dimming

    uint8_t disable_masks_num;     // Number of entries in the array
    HA_EEMEM const uint8_t *disable_masks;  // Array of leds ch on/off mask to roll over while disable roll
} ha_node_ll_cfg_t;

 // Message Data offsets
#define LL_DATA_OFF_MODE        0
#define LL_DATA_OFF_DIS_MASK    1
#define LL_DATA_OFF_INTENSITY0  2
//#define LL_DATA_OFF_INTENSITY1  3
//#define LL_DATA_OFF_INTENSITY2  4
//#define LL_DATA_OFF_INTENSITY3  5
//#define LL_DATA_LEN         (LL_DATA_OFF_INTENSITY0  + LEDS_NUM) // Max packet length depends on configuration

enum LED_MODE {
    LED_MODE_X3 = 0,
    LED_MODE_ON = 2,
    LED_MODE_OFF = 3,
    LED_MODE_ON_TRANS = 4,
    LED_MODE_OFF_TRANS = 5
};

typedef enum ha_node_ll_act_type_e {
    LL_ACT_TYPE_NONE,       // is used as End of Action Table mark
    LL_ACT_TYPE_ON,
    LL_ACT_TYPE_OFF,
    LL_ACT_TYPE_TOGGLE,
    LL_ACT_TYPE_ROLL,
    LL_ACT_TYPE_DIMM,
    LL_ACT_TYPE_ROLLDIMM,
} ha_node_ll_act_type_t;

#define LED_FADEIN_STEP_PERIOD     2     // in timer ticks
#define LED_FADEOUT_STEP_PERIOD    2     // in timer ticks

// INFO structure for particular light source
typedef struct {
    uint8_t  uc_current_intensity_idx;
    uint8_t  uc_target_intensity_idx;
    uint8_t  uc_on_intensity_idx;
    uint8_t  uc_dimm_idx;
    uint8_t  uc_fade_timer;
    uint8_t  uc_ch_mask;
} led_info_t;

typedef struct ha_node_ll_evt_action_s {
    uint8_t evt_type;
    uint8_t evt_addr;
    uint8_t evt_param;
    ha_node_ll_act_type_t act_type;
    uint8_t act_param;
} ha_node_ll_evt_action_t;


#define HA_NODE_LL_EVT_ACTION_EOT    { 0, 0,  0,  LL_ACT_TYPE_NONE,  0 }

typedef struct ha_node_ll_info_s {
    const ha_node_ll_evt_action_t *evt_actions;
    HA_EEMEM const ha_node_ll_cfg_t *cfg;
    node_t *node;
    uint8_t  disabled_idx;
    uint8_t  disabled_mask;
    uint8_t  led_mode;

    uint8_t leds_num;
    led_info_t *leds;
    
    const ha_node_ll_evt_action_t *action;  /* Action to execute in idle loop. Points to the evt_action table above */
} ha_node_ll_info_t;                        /* 14 */

extern ha_node_ll_info_t *ha_node_ledlight_create (const ha_node_ll_cfg_t *cfg, const ha_node_ll_evt_action_t *cfg_evt_actions);
extern void ha_node_ledlight_on_timer(ha_node_ll_info_t *ll);
extern void ha_node_ledlight_on_idle(ha_node_ll_info_t *ll);

/************************************************************************/
/* Need to be implemented by the integration                            */
/************************************************************************/
extern void ha_node_ledlight_set_intensity(uint8_t led_mask, uint8_t intensity_idx);



