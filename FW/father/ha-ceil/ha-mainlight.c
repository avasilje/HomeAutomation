// TODO: !!!
// +1. Connect power supply
// +2. Connect debugger
// +3. Debug Switch Function
// +4. Debug slow PWM (PINA4, PINA5)
// +5. Assemble and solder-in Switching Extension Board
// 6. Connect to NLINK 
// 7. Legalize Switch & LedLighth nodes in the APP

#include <stdlib.h>
#include <string.h>
#include "ha-common.h"
#include "ha-nlink.h"
#include "ha-node-switch.h"
#include "ha-node-ledlight.h"

#include "ha-mainlight.h"
#include "ha-mainlight-const.h"
#include "ha-ceil-config.h"

/*
* ha-mainlight.c
*
* Created: 17/03/2019 9:49:58 AM
*  Author: Solit
*/
uint16_t g_led_intensity_cnt = 0;
uint8_t guc_led_intenisity_timer = 0;

uint16_t g_switches_cnt = 0;
uint8_t guc_switches_timer = 0;

// ------- EEPROM ---------------
uint8_t euca_signature[2] __attribute__ ((section (".config_param")));
//uint8_t euca_standby_intensity[LEDS_NUM] __attribute__ ((section (".config_param")));
//uint8_t euca_on_intensity[LEDS_NUM] __attribute__ ((section (".config_param")));

uint8_t euc_leds_disabled_idx __attribute__ ((section (".config_param")));

uint8_t  guc_sw_event_mask;     //
uint8_t  guc_curr_led_mode;     //

#define EEPROM_SAVE_FLAG_DISABLED  1
#define EEPROM_SAVE_FLAG_INTENSITY 2
uint8_t  guc_eeprom_save_flags;

#ifndef DBG_EN
#define DBG_EN 1
#endif

#if DBG_EN
#define DBG_VAR_NUM 24
uint16_t dbg_p[DBG_VAR_NUM];
uint8_t dbg_idx = 0;
#define DBG_TRACE(x) \
    do {dbg_p[dbg_idx++] = x; if (dbg_idx == DBG_VAR_NUM) dbg_idx = 0; } while(0)
#endif


uint16_t gus_trap_line;

ha_node_ll_info_t  *ll_hall_spt = NULL; 
ha_node_ll_info_t  *ll_room_amb = NULL; 
ha_node_ll_info_t  *ll_room_spt_mid = NULL;
ha_node_ll_info_t  *ll_room_spt_bed = NULL;

ha_node_sw_info_t *sw_ceil = NULL;

uint8_t ha_node_switch_get_pins() 
{
    return ha_dev_base_get_in_pins();
}

void ha_node_ledlight_set_intensity (uint8_t led_mask, uint8_t intensity_idx)
{
    if (intensity_idx >= INTENSITIES_NUM) intensity_idx = INTENSITIES_NUM - 1;

    if (intensity_idx == 0) {
        DBG_TRACE(0x11);
        ha_dev_base_set_steady(led_mask, 0);
    } else if (intensity_idx == INTENSITIES_NUM - 1) {
        ha_dev_base_set_steady(led_mask, 1);
        DBG_TRACE(0x77);
    } else {
        DBG_TRACE(intensity_idx);
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

    ll_room_amb     = ha_node_ledlight_create(&room_amb_ll_cfg,     &room_amb_ll_action[0]);
    ll_hall_spt     = ha_node_ledlight_create(&hall_spt_ll_cfg,     &hall_spt_ll_action[0]);
    ll_room_spt_mid = ha_node_ledlight_create(&room_spt_mid_ll_cfg, &room_spt_mid_ll_action[0]);
    ll_room_spt_bed = ha_node_ledlight_create(&room_spt_bed_ll_cfg, &room_spt_bed_ll_action[0]);
    
    sw_ceil         = ha_node_switch_create(&ceil_sw_cfg);
   

    sei();

    // Endless loop
    while(1)
    {
        __asm__ __volatile__ ("    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"
                                ::);
                                
        ha_node_ledlight_on_idle(ll_hall_spt);
        ha_node_ledlight_on_idle(ll_room_amb);
        ha_node_ledlight_on_idle(ll_room_spt_mid);
        ha_node_ledlight_on_idle(ll_room_spt_bed);
                                
        ha_nlink_check_rx();
        ha_nlink_check_tx();
        
        if (guc_led_intenisity_timer) {
            guc_led_intenisity_timer = 0;
            ha_node_ledlight_on_timer(ll_hall_spt);
            ha_node_ledlight_on_timer(ll_room_amb);
            ha_node_ledlight_on_timer(ll_room_spt_mid);
            ha_node_ledlight_on_timer(ll_room_spt_bed);
        }
        
        if (guc_switches_timer) {
            guc_switches_timer = 0;
            ha_node_switch_on_timer(sw_ceil);
        }
    
#if 0
// TODO: finalize code below
        if (guc_eeprom_save_flags) {
            for (uint8_t uc_led = 0; uc_led < LEDS_NUM; uc_led++) {
                eeprom_write_byte(&euca_led_disabled[uc_led], gta_leds[uc_led].uc_disabled);
            }
            for (uc_led = 0; uc_led < LEDS_NUM; uc_led++) {
                eeprom_write_byte(&euca_on_intensity[uc_led], gta_leds[uc_led].uc_on_intensity);
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

