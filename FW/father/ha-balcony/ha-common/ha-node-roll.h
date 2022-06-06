/*
 * ha_node_roll.h
 *
 * Created: 28/12/2021
 *  Author: Solit
 */
#pragma once

typedef struct roll_ch_mask_s {
    uint8_t up;
    uint8_t down;
} roll_ch_mask_t;    

typedef struct ha_node_roll_cfg_s {
    uint8_t node_addr;
    uint8_t rolls_num;
    const roll_ch_mask_t *ee_ch_mask; 
    uint16_t active_timeout;
} ha_node_roll_cfg_t;

 // Message Data offsets
#define ROLL_DATA_OFF_MODE 0

typedef struct {
    uint8_t ch_idx;        // +To access to the ROLL ch_mask from config tables
} roll_info_t;

typedef enum ha_node_roll_act_type_e {
    ROLL_ACT_TYPE_NONE = 0,
    ROLL_ACT_TYPE_UP   = 1,
    ROLL_ACT_TYPE_DOWN = 2,
    ROLL_ACT_TYPE_STOP = 3,
} ha_node_roll_act_type_t;

typedef struct ha_node_roll_evt_action_s {
    uint8_t evt_type;
    uint8_t evt_addr;
    uint8_t evt_param;
    ha_node_roll_act_type_t act_type;
    uint8_t act_param;
} ha_node_roll_evt_action_t;

#define HA_NODE_ROLL_EVT_ACTION_EOT    { 0, 0,  0,  ROLL_ACT_TYPE_NONE,  0 }

#define ROLL_SWITCH_DELAY 10        // in 10ms ticks

enum roll_mode_e {
    ROLL_MODE_STOP      = 0,
    ROLL_MODE_UP        = 1,
    ROLL_MODE_DOWN      = 2,
};

typedef struct ha_node_roll_info_s {
    const ha_node_roll_evt_action_t *cfg_evt_actions;
    const ha_node_roll_cfg_t *cfg;

    node_t *node;

    enum roll_mode_e curr_mode;     // Going to DOWN, going to UP - mode includes transient states
    enum roll_mode_e trgt_mode;
    uint16_t mode_change_delay;      // in 10ms

    uint8_t rolls_num;
    roll_info_t *rolls;

} ha_node_roll_info_t;

extern ha_node_roll_info_t *ha_node_roll_create (const ha_node_roll_cfg_t *cfg, const ha_node_roll_evt_action_t *cfg_evt_actions);
extern void ha_node_roll_on_timer(ha_node_roll_info_t *node_roll);
extern void ha_node_roll_on_idle (ha_node_roll_info_t *node_roll);

/************************************************************************/
/* Need to be implemented by the integration                            */
/************************************************************************/
extern void ha_node_roll_set_mode (const roll_ch_mask_t *ee_ch_mask, enum roll_mode_e roll_mode);



