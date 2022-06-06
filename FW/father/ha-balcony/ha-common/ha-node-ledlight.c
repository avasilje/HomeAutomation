
#include <string.h>
#include <stdlib.h>

#include <avr/eeprom.h>

#include "ha-common.h"
#include "ha-nlink.h"
#include "ha-node-ledlight.h"

/*
* ha-ledlight.c
*
* Created: 17/03/2019 9:49:58 AM
*  Author: Solit
*/

#define EEPROM_SAVE_FLAG_DISABLED  1
#define EEPROM_SAVE_FLAG_INTENSITY 2

#ifndef DBG_EN
#define DBG_EN 0
#endif

#if DBG_EN
#define DBG_VAR_NUM 24
uint16_t dbg_p[DBG_VAR_NUM];
uint8_t dbg_idx = 0;
#endif

uint16_t gus_trap_line;

void ha_node_ledlight_on_param (ha_node_ll_info_t *ll, uint8_t param_intensity_idx)
{ // LVL3 + 3 + 2
    uint8_t led_mask = 1;
    node_t *node = ll->node;
    uint8_t fadein_period = ll->cfg->fadein_period;

    for (uint8_t i = 0; i < ll->leds_num; i ++, led_mask <<= 1) {
        led_info_t *led = &ll->leds[i];
        // If intensity not specified, then use the current one
        // otherwise set explicitly specified intensity
        uint8_t intensity_idx = (param_intensity_idx == INTENSITIES_NUM) ?
            led->uc_on_intensity_idx : param_intensity_idx;

        led->uc_target_intensity_idx = led_mask & ll->disabled_mask ? 0 : intensity_idx;
        led->uc_fade_timer = fadein_period;
    }

    node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_OFF_MODE] = LED_MODE_ON;        // mode is not ON yet, but will be soon
    ha_nlink_node_send(node, NODE_ADDR_BC, NLINK_CMD_INFO);
}

#define ha_node_ledlight_on(ll) ha_node_ledlight_on_param(ll, INTENSITIES_NUM)

void ha_node_ledlight_off (ha_node_ll_info_t *node_ll)
{
    // LVL +2 +2    ha_node_ledlight_off 
    
    node_t *node = node_ll->node;
    uint8_t fadeout_period = node_ll->cfg->fadeout_period;
    for (uint8_t i = 0; i < node_ll->leds_num; i ++) {
        led_info_t *led = &node_ll->leds[i];
        led->uc_target_intensity_idx = 0;
        led->uc_fade_timer = fadeout_period;
    }
    node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_OFF_MODE] = LED_MODE_OFF;        // mode is not OFF yet, but will be soon
    ha_nlink_node_send(node, NODE_ADDR_BC, NLINK_CMD_INFO);
}

/*
 * If dimm_idx == dimms_num then roll over dimm table,
 * otherwise set dimm_idx to the specified value
 */
void ha_node_ledlight_dimm (ha_node_ll_info_t *node_ll, uint8_t dimm_idx)
{ // LVL3    +11 +2
  //   LVL4      +2  eeprom_read
    
    node_t *node = node_ll->node;
    uint8_t dimms_num = node_ll->cfg->dimms_num;
    uint8_t fadeout_period = node_ll->cfg->fadeout_period;
    uint8_t all_off = 0;

    for (uint8_t i = 0; i < node_ll->leds_num; i ++) {
        led_info_t *led = &node_ll->leds[i];
        if (dimm_idx == dimms_num) {
            // Rollover dimm table
            if (led->uc_dimm_idx == 0) {
                all_off ++;
            } else {
                led->uc_dimm_idx--;
            }
        } else {
            led->uc_dimm_idx = dimm_idx;
        }
        
        led->uc_on_intensity_idx = eeprom_read_byte(node_ll->cfg->dimms + led->uc_dimm_idx);
        led->uc_fade_timer = fadeout_period;
    }

    // Wrap around intensity only when LED are OFF
    if (all_off == node_ll->leds_num) {
        uint8_t last_idx = dimms_num - 1;
        uint8_t last_intensity = eeprom_read_byte(node_ll->cfg->dimms + last_idx);
        for (uint8_t i = 0; i < node_ll->leds_num; i ++) {
            led_info_t *led = &node_ll->leds[i];
            led->uc_dimm_idx = last_idx;
            led->uc_on_intensity_idx = last_intensity;
        }
    }

    // Update NLINK buffer
    for (uint8_t i = 0; i < node_ll->leds_num; i ++) {
        led_info_t *led = &node_ll->leds[i];
        node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_OFF_INTENSITY0 + i] = led->uc_on_intensity_idx;
    }
    return;
}

void ha_node_ledlight_roll(ha_node_ll_info_t *node_ll)
{ // LVL3     +4 +2
  //   LVL4      +2 eeprom
  
    node_t *node = node_ll->node;

    if (node_ll->cfg->disable_masks == NULL || node_ll->cfg->disable_masks_num == 0)
        return;

    node_ll->disabled_idx++;
    if (node_ll->disabled_idx == node_ll->cfg->disable_masks_num) node_ll->disabled_idx = 0;
    node_ll->disabled_mask = eeprom_read_byte(node_ll->cfg->disable_masks + node_ll->disabled_idx);
    node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_OFF_DIS_MASK ] = node_ll->disabled_mask;
    
    return;
}

void ha_node_ledlight_on_idle(ha_node_ll_info_t *node_ll)
{ // LVL2        +2  +2
  //   LVL3      +11 +2   ha_node_ledlight_dimm
  //     LVL4        +2   eeprom_read

  //   LVL3      +2  +2   ha_node_ledlight_on_param
  
    uint8_t act_type;
    uint8_t act_param;

    if (!node_ll->action) return;
    
    act_type = node_ll->action->act_type;
    act_param = node_ll->action->act_param;
    
    node_ll->action = NULL;
    
    switch(act_type) {
    case LL_ACT_TYPE_TOGGLE:
        if (node_ll->led_mode == LED_MODE_ON) {
            node_ll->led_mode = LED_MODE_OFF_TRANS;
            ha_node_ledlight_off(node_ll);
        } else if (node_ll->led_mode == LED_MODE_OFF) {
            node_ll->led_mode = LED_MODE_ON_TRANS;
            ha_node_ledlight_on(node_ll);
        }
        break;
    case LL_ACT_TYPE_ROLLDIMM:
        if (node_ll->led_mode == LED_MODE_ON) {
            ha_node_ledlight_dimm(node_ll, 0);
            ha_node_ledlight_on(node_ll);
        } else if (node_ll->led_mode == LED_MODE_OFF) {
            ha_node_ledlight_roll(node_ll);
            ha_node_ledlight_on(node_ll);
        }
        break;
    case LL_ACT_TYPE_DIMM:
        node_ll->led_mode = LED_MODE_ON_TRANS;
        ha_node_ledlight_dimm(node_ll, act_param);
        ha_node_ledlight_on(node_ll);
        break;
    case LL_ACT_TYPE_ON:
        node_ll->led_mode = LED_MODE_ON_TRANS;
        ha_node_ledlight_on_param(node_ll, act_param);
        break;
    }

    return;
}

static void sw_behavior_control(ha_node_ll_info_t *node_ll, uint8_t addr_from, uint8_t sw_param)
{ // LVL - static
    
    // Traverse over action table and check all entries of type SWITCH
    for( const ha_node_ll_evt_action_t *evt_act = node_ll->evt_actions; 
         evt_act->act_type != LL_ACT_TYPE_NONE;
         evt_act++ ) {

        if (evt_act->evt_type != NODE_TYPE_SWITCH)
            continue;

        if (evt_act->evt_addr != addr_from && evt_act->evt_addr != NODE_ADDR_BC)
            continue;

        if (evt_act->evt_param != sw_param) 
            continue;

        /* Action is recognized by LL - postpone execution
           to the idle loop to reduce stack usage  */
        node_ll->action = evt_act;
        return;       

    } // End of action table traverse loop
    
}

void ledlight_on_rx (void *ctx, const uint8_t *buf_in)
{ 
    // LVL3     +7  +2    ledlight_on_rx
    //   LVL4   +2  +2    ha_node_ledlight_off 
    
    ha_node_ll_info_t *node_ll = (ha_node_ll_info_t *)ctx;
    node_t *node = node_ll->node;
    int i;

    uint8_t addr_from = buf_in[NLINK_HDR_OFF_FROM];
    uint8_t len = buf_in[NLINK_HDR_OFF_LEN];

    if (buf_in[NLINK_HDR_OFF_TYPE] == NODE_TYPE_LEDLIGHT) {
        // Direct LEDLIGHT state info. Typically received from user console
        // Address must be specified explicitly
        if (buf_in[NLINK_HDR_OFF_TO] != node_ll->node->addr) {
            return;
        }

        node_ll->led_mode = buf_in[NLINK_HDR_OFF_DATA + LL_DATA_OFF_MODE];
        node_ll->disabled_mask = buf_in[NLINK_HDR_OFF_DATA + LL_DATA_OFF_DIS_MASK];
        for (i = 0; i < node_ll->leds_num; i++) {
            node_ll->leds[i].uc_on_intensity_idx = buf_in[NLINK_HDR_OFF_DATA + LL_DATA_OFF_INTENSITY0 + i];
        }

        // Update node TX buffer to reflect changes in outgoing messages
        uint8_t ll_data_len = LL_DATA_OFF_INTENSITY0 + node_ll->leds_num;
        memcpy(&node->tx_buf[NLINK_HDR_OFF_DATA], &buf_in[NLINK_HDR_OFF_DATA], ll_data_len);

        if (node_ll->led_mode == LED_MODE_ON) {
            ha_node_ledlight_on(node_ll);
        } else if (node_ll->led_mode == LED_MODE_OFF) {
            ha_node_ledlight_off(node_ll);
        }

    } else if (buf_in[NLINK_HDR_OFF_TYPE] == NODE_TYPE_SWITCH) {

        // Switch event
        for (uint8_t i = 0; i < len; i++) {
            uint8_t sw_param = buf_in[NLINK_HDR_OFF_DATA + i];
            sw_behavior_control(node_ll, addr_from, sw_param);
        }

    } else {
        // Unexpected event type
        return;
    }
    
    return;
}

ha_node_ll_info_t *ha_node_ledlight_create (const ha_node_ll_cfg_t *cfg, const ha_node_ll_evt_action_t *cfg_evt_actions)
{
    int i;
    led_info_t *leds = (led_info_t*)calloc(cfg->leds_num, sizeof(led_info_t));
    ha_node_ll_info_t *node_ll = (ha_node_ll_info_t*)calloc(1, sizeof(ha_node_ll_info_t));
    node_t *node;
    uint8_t max_intensity;

    // Check configuration
    for( const ha_node_ll_evt_action_t *evt_act = cfg_evt_actions; 
         evt_act->act_type != LL_ACT_TYPE_NONE;
         evt_act++ ) {

        switch (evt_act->act_type) {
            case LL_ACT_TYPE_DIMM:
                if (evt_act->act_param > cfg->dimms_num) return NULL;
                break;
            case LL_ACT_TYPE_ON: 
                if (evt_act->act_param > INTENSITIES_NUM) return NULL;
                break;
            default:
                break;
        }
    }

    node_ll->evt_actions = cfg_evt_actions;
    node_ll->cfg = cfg;
    node_ll->leds = leds;
    node_ll->leds_num = cfg->leds_num;

    node = ha_nlink_node_register(cfg->node_addr, NODE_TYPE_LEDLIGHT, ledlight_on_rx, node_ll);
    node_ll->node = node;

    // Switch LEDs off on startup
    node_ll->led_mode = LED_MODE_OFF_TRANS;

    node_ll->disabled_idx = 0;                      // All Enabled
    node_ll->disabled_mask = eeprom_read_byte(cfg->disable_masks + node_ll->disabled_idx);
    
    max_intensity = eeprom_read_byte(cfg->dimms + cfg->dimms_num - 1);  // Max intensity by default    
    
    for (i = 0; i < node_ll->leds_num; i++) {
        led_info_t *led = &node_ll->leds[i];
        led->uc_on_intensity_idx = max_intensity;
        led->uc_ch_mask = eeprom_read_byte(cfg->leds_ch_masks + i);
    }

    node->tx_buf[NLINK_HDR_OFF_TYPE] = NODE_TYPE_LEDLIGHT;
    node->tx_buf[NLINK_HDR_OFF_LEN] = LL_DATA_OFF_INTENSITY0 + node_ll->leds_num;
    // ll_node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_MODE     ] = LED_MODE_OFF;     // Is set unconditionally on every packet send
    node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_OFF_DIS_MASK ] = node_ll->disabled_mask;

    for (i = 0; i < node_ll->leds_num; i++) {
        led_info_t *led = &node_ll->leds[i];
        node->tx_buf[NLINK_HDR_OFF_DATA + LL_DATA_OFF_INTENSITY0 + i] = led->uc_on_intensity_idx;
    }

    ha_node_ledlight_off(node_ll);
    return node_ll;
}

void ha_node_ledlight_on_timer(ha_node_ll_info_t   *node_ll)
{ 
    // LVL        +4 +2   ha_node_ledlight_on_timer
    //   LVL      +0 +2   ha_node_ledlight_set_intensity
    //     LVL    +2 +2   ha_dev_leds_set_fast_pwm
    //       LVL     +2   eeprom
  
  
    uint8_t uc_target_intensity, uc_current_intensity;
    uint8_t uc_leds_steady = 1;

    if (node_ll == NULL) return;

    // Loop over all LEDs
    for (uint8_t i = 0; i < node_ll->leds_num; i ++) {
        led_info_t *led = &node_ll->leds[i];

        uc_target_intensity = led->uc_target_intensity_idx;
        uc_current_intensity = led->uc_current_intensity_idx;

        if (uc_target_intensity == uc_current_intensity) {
            continue;
        }

        uc_leds_steady = 0;

        // LEDs' intensity are in transition
        // check fade timer
        led->uc_fade_timer --;
        if ( led->uc_fade_timer == 0) {
            // Its time to update fade in/out
            if (uc_target_intensity > uc_current_intensity) {
                // FADE IN
                uc_current_intensity ++;
                led->uc_fade_timer = LED_FADEIN_STEP_PERIOD;
            }
            else if (uc_target_intensity < uc_current_intensity) {
                // FADE OUT
                uc_current_intensity --;
                led->uc_fade_timer = LED_FADEOUT_STEP_PERIOD;
            }

            led->uc_current_intensity_idx = uc_current_intensity;
            ha_node_ledlight_set_intensity(led->uc_ch_mask, uc_current_intensity);
        } // End of FADE IN/OUT

    } // End all LEDs loop

    // ----------------------------------------------
    // --- Transient states
    // ----------------------------------------------
    if (node_ll->led_mode == LED_MODE_ON_TRANS) {
        if (uc_leds_steady) {
            node_ll->led_mode = LED_MODE_ON;
        }
    } // End of TRANSITION to ON

    else if (node_ll->led_mode == LED_MODE_OFF_TRANS) {
        if (uc_leds_steady) {
            node_ll->led_mode = LED_MODE_OFF;
        }
        return;
    } // End of TRANSITION to OFF

    return;
}

