
#include <string.h>
#include <stdlib.h>

#include "ha-common.h"
#include "ha-nlink.h"
#include "ha-node-roll.h"

/*
* ha-node-roll.c
*
* Created: 28/12/2021
*  Author: Solit
*/
#ifndef DBG_EN
#define DBG_EN 0
#endif

#if DBG_EN
#define DBG_VAR_NUM 24
uint16_t dbg_p[DBG_VAR_NUM];
uint8_t dbg_idx = 0;
#endif

static void roll_sw_behavior_control(ha_node_roll_info_t *roll_node, uint8_t addr_from, uint8_t sw_param)
{    
    // Traverse over action table and check all entries of type SWITCH
    for( const ha_node_roll_evt_action_t *evt_act = roll_node->cfg_evt_actions; 
         evt_act->act_type != ROLL_ACT_TYPE_NONE;
         evt_act++ ) {

        if (evt_act->evt_type != NODE_TYPE_SWITCH)
            continue;

        if (evt_act->evt_addr != addr_from && evt_act->evt_addr != NODE_ADDR_BC)
            continue;

        if (evt_act->evt_param != sw_param) 
            continue;
       
        roll_node->trgt_mode =
            (evt_act->act_type == ROLL_ACT_TYPE_UP)   ? ROLL_MODE_UP :
            (evt_act->act_type == ROLL_ACT_TYPE_DOWN) ? ROLL_MODE_DOWN : ROLL_MODE_STOP;
        
        roll_node->mode_change_delay = ROLL_SWITCH_DELAY;
        return;       
    } // End of action table traverse loop
}

void roll_on_rx (void *ctx, const uint8_t *buf_in)
{ 
    ha_node_roll_info_t *roll_node = (ha_node_roll_info_t *)ctx;

    uint8_t addr_from = buf_in[NLINK_HDR_OFF_FROM];
    uint8_t len = buf_in[NLINK_HDR_OFF_LEN];

    if (buf_in[NLINK_HDR_OFF_TYPE] == NODE_TYPE_ROLL) {
        if (buf_in[NLINK_HDR_OFF_TO] != roll_node->node->addr) {
            return;
        }
        
        roll_node->trgt_mode = buf_in[NLINK_HDR_OFF_DATA + ROLL_DATA_OFF_MODE];
        roll_node->mode_change_delay = ROLL_SWITCH_DELAY;
        
    } else if (buf_in[NLINK_HDR_OFF_TYPE] == NODE_TYPE_SWITCH) {

        // Switch event
        for (uint8_t i = 0; i < len; i++) {
            uint8_t sw_param = buf_in[NLINK_HDR_OFF_DATA + i];
            roll_sw_behavior_control(roll_node, addr_from, sw_param);
        }

    } else {
        // Unexpected event type
        return;
    }
    
    return;
}

ha_node_roll_info_t *ha_node_roll_create (const ha_node_roll_cfg_t *cfg, const ha_node_roll_evt_action_t *cfg_evt_actions)
{
    roll_info_t *rolls = (roll_info_t*)calloc(cfg->rolls_num, sizeof(roll_info_t));
    ha_node_roll_info_t *roll_node = (ha_node_roll_info_t*)calloc(1, sizeof(ha_node_roll_info_t));
    node_t *node;

    // Check configuration
    for( const ha_node_roll_evt_action_t *evt_act = cfg_evt_actions; 
         evt_act->act_type != ROLL_ACT_TYPE_NONE;
         evt_act++ ) {

        switch (evt_act->act_type) {
            case ROLL_ACT_TYPE_DOWN:
            case ROLL_ACT_TYPE_UP:
            case ROLL_ACT_TYPE_STOP:
            default:
                break;
        }
    }

    roll_node->cfg_evt_actions = cfg_evt_actions;
    roll_node->cfg = cfg;
    roll_node->rolls = rolls;
    roll_node->rolls_num = cfg->rolls_num;

    node = ha_nlink_node_register(cfg->node_addr, NODE_TYPE_ROLL, roll_on_rx, roll_node);
    roll_node->node = node;

    roll_node->curr_mode = -1;
    roll_node->trgt_mode = ROLL_MODE_STOP;
    roll_node->mode_change_delay = 0;

    node->tx_buf[NLINK_HDR_OFF_TYPE] = NODE_TYPE_ROLL;
    node->tx_buf[NLINK_HDR_OFF_LEN] = ROLL_DATA_OFF_MODE + roll_node->rolls_num;
    // node->tx_buf[NLINK_HDR_OFF_DATA + ROLL_DATA_OFF_MODE     ] = xxx;     // Is set unconditionally on every packet send

    return roll_node;
}

void ha_node_roll_on_timer(ha_node_roll_info_t *roll_node)
{ 
    
    if (roll_node == NULL) return;

    if (roll_node->curr_mode == roll_node->trgt_mode) return;
    
    if (roll_node->mode_change_delay) {
        roll_node->mode_change_delay --;
        return;
    }

    if (roll_node->curr_mode == ROLL_MODE_STOP) {
        roll_node->curr_mode = roll_node->trgt_mode;
        
        // Force stop after some time        
        roll_node->trgt_mode = ROLL_MODE_STOP;
        roll_node->mode_change_delay = roll_node->cfg->active_timeout;
    } else {
        // Stop before direction change
        roll_node->curr_mode = ROLL_MODE_STOP;
        roll_node->mode_change_delay = ROLL_SWITCH_DELAY;
    }
    
    for (uint8_t i = 0; i < roll_node->rolls_num; i++) {
        ha_node_roll_set_mode(&roll_node->cfg->ee_ch_mask[i], roll_node->curr_mode);
    }
        
    roll_node->node->tx_buf[NLINK_HDR_OFF_DATA + ROLL_DATA_OFF_MODE] = roll_node->curr_mode;
    ha_nlink_node_send(roll_node->node, NODE_ADDR_BC, NLINK_CMD_INFO);
    
    return;
}    
