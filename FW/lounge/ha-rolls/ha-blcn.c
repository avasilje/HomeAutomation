#include <stdlib.h>
#include <string.h>

#include <avr/eeprom.h>

#include "ha-common.h"
#include "ha-nlink.h"
#include "ha-node-switch.h"
#include "ha-node-ledlight.h"
#include "ha-node-roll.h"

#include "ha-blcn-config.h"

/*
*
* Created: 17/03/2019 9:49:58 AM
*  Author: Solit
*/
uint16_t g_led_intensity_cnt = 0;
uint8_t guc_led_intenisity_timer = 0;

uint16_t g_switches_cnt = 0;
uint8_t guc_switches_timer = 0;

#ifndef DBG_EN
#define DBG_EN 0
#endif

#if DBG_EN
#define DBG_VAR_NUM 24
uint16_t dbg_p[DBG_VAR_NUM];
uint8_t dbg_idx = 0;
#endif

uint16_t gus_trap_line;

ha_node_ll_info_t  *ll_blcn = NULL; 
ha_node_sw_info_t *sw_blcn = NULL;
ha_node_roll_info_t *roll_d_blcn = NULL;
ha_node_roll_info_t *roll_w_blcn = NULL;

uint8_t ha_node_switch_get_pins() 
{
    return ha_dev_base_get_in_pins();
}

void ha_node_roll_set_mode (const roll_ch_mask_t *ee_ch_mask, enum roll_mode_e roll_mode)
{
    roll_ch_mask_t ch_mask;
    eeprom_read_block(&ch_mask, ee_ch_mask, sizeof(roll_ch_mask_t));
    
    switch (roll_mode) {
        case ROLL_MODE_UP:
            ha_dev_base_set_steady(ch_mask.down, 0);
            ha_dev_base_set_steady(ch_mask.up,   1);
            break;
        case ROLL_MODE_DOWN:
            ha_dev_base_set_steady(ch_mask.up,   0);
            ha_dev_base_set_steady(ch_mask.down, 1);
            break;
        case ROLL_MODE_STOP:
            ha_dev_base_set_steady(ch_mask.down, 0);
            ha_dev_base_set_steady(ch_mask.up,   0);
            break;
        break;
    }
}

void ha_node_ledlight_set_intensity (uint8_t led_mask, uint8_t intensity_idx)
{
    if (intensity_idx >= INTENSITIES_NUM) intensity_idx = INTENSITIES_NUM - 1;

    if (intensity_idx == 0) {
        ha_dev_base_set_steady(led_mask, 0);
    } else if (intensity_idx == INTENSITIES_NUM - 1) {
        ha_dev_base_set_steady(led_mask, 1);
    } else {
        ha_dev_base_set_fast_pwm(led_mask, intensity_idx);
    }
}

int main(void)
{
    // Wait a little just in case
    for(uint8_t uc_i = 0; uc_i < 255U; uc_i++){

        __asm__ __volatile__ ("    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"
                                ::);
    }

    ha_dev_base_init();

    ha_nlink_init();

    ll_blcn   = ha_node_ledlight_create(&blcn_ll_cfg, &blcn_ll_action[0]);
    sw_blcn   = ha_node_switch_create (&blcn_sw_cfg);
    roll_d_blcn = ha_node_roll_create (&blcn_roll_d_cfg, &blcn_roll_d_action[0]);
    roll_w_blcn = ha_node_roll_create (&blcn_roll_w_cfg, &blcn_roll_w_action[0]);

    sei();

    // Endless loop
    while(1)
    {
        __asm__ __volatile__ ("    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"
                                ::);
                                
        ha_node_ledlight_on_idle(ll_blcn);
                               
        ha_nlink_check_rx();
        ha_nlink_check_tx();
        
        if (guc_led_intenisity_timer) {
            // 10ms timer
            guc_led_intenisity_timer = 0;
            ha_node_ledlight_on_timer(ll_blcn);
            ha_node_roll_on_timer(roll_d_blcn);
            ha_node_roll_on_timer(roll_w_blcn);
        }
        
        if (guc_switches_timer) {
            // 10ms timer
            guc_switches_timer = 0;
            ha_node_switch_on_timer(sw_blcn);
        }

    
#if 0
// TODO: finalize code below
        if (guc_eeprom_save_flags) {
            for (uint8_t uc_led = 0; uc_led < LEDS_NUM; uc_led++) {
                eeprom_write_byte(&euca_led_disabled[uc_led], 
                    gta_leds[uc_led].uc_disabled);
            }
            for (uc_led = 0; uc_led < LEDS_NUM; uc_led++) {
                eeprom_write_byte(&euca_on_intensity[uc_led], 
                    gta_leds[uc_led].uc_on_intensity);
            }
        }
#endif
    }
}

/************************************************************************/
/* Att!: Called from ISR context                                        */
/************************************************************************/
void isr_ha_app_on_timer()
{
    // AV: Need to be reworked for 10ms timer common for all nodes types
    // Called every 12.8usec
    
    // Set intensity timer every ~10ms ~= 781 * 12.8 = 9996.8us = 9.99ms
    g_led_intensity_cnt++;
    if (g_led_intensity_cnt == 781) {
        g_led_intensity_cnt = 0;
        guc_led_intenisity_timer = 1;
    }

    // Set switch timer every ~10ms ~= 781 * 12.8 = 9996.8us = 9.99ms
    g_switches_cnt++;
    if (g_switches_cnt == 781) {
        g_switches_cnt = 0;
        guc_switches_timer = 1;
    }

}

