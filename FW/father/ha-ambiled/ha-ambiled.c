/* Ram 15byte ISR 
        -Wl,--defsym,STACK_SIZE=0x20 ???
 */
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/eeprom.h>

//#include <stdlib.h>
#include <string.h>
#include "ha-common.h"
#include "ha-nlink.h"
#include "ha-node-switch.h"
#include "ha-node-ledlight.h"

#include "ha-ambiled.h"
#include "ha-ambiled-const.h"
#include "ha-ambiled-config.h"

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

ha_node_ll_info_t  *ll_ambiled = NULL;
ha_node_sw_info_t  *sw_ambiled = NULL;

#ifndef DBG_EN
#define DBG_EN 0
#endif

#if DBG_EN
#define DBG_VAR_NUM 24
uint16_t dbg_p[DBG_VAR_NUM];
uint8_t dbg_idx = 0;
#endif

#ifdef NO_MALLOC
uint8_t g_heap[54];     /* 24 + 14 + 7 + 7 = 52 bytes */

void * _calloc(int n, int s)
{
    static uint8_t *heap_ptr = &g_heap[0];
    uint8_t *mem_ptr = heap_ptr;
    heap_ptr += n*s;
    return mem_ptr;
}
#define _free(p) 

#else
#define _calloc(x, y) calloc(x, y)
#define _free(x) free(x)
#endif

uint8_t ha_node_switch_get_pins()
{
    return ha_dev_leds_get_in_pins();
}

void ha_node_ledlight_set_intensity (uint8_t led_mask, uint8_t intensity_idx)
{ 
    // LVL      +0 +2   ha_node_ledlight_set_intensity
    //   LVL    +2 +2   ha_dev_leds_set_fast_pwm
    //     LVL     +2   eeprom
    
    
    if (intensity_idx >= INTENSITIES_NUM) intensity_idx = INTENSITIES_NUM - 1;

    if (intensity_idx == 0) {
        ha_dev_leds_set_steady(led_mask, 0);
    } else if (intensity_idx == INTENSITIES_NUM - 1) {
        ha_dev_leds_set_steady(led_mask, 1);
    } else {
        ha_dev_leds_set_fast_pwm(led_mask, intensity_idx);
    }
}

int main(void)
{ 
    // LVL1              +2   ha_node_ledlight_on_idle
    //   LVL2        +2  +2
    //     LVL3      +11 +2   ha_node_ledlight_dimm
    //       LVL4        +2   eeprom_read
    // ==============   +21   =============
    
    // LVL            +4 +2   ha_node_ledlight_on_timer
    //   LVL          +0 +2   ha_node_ledlight_set_intensity
    //     LVL        +2 +2   ha_dev_leds_set_fast_pwm
    //       LVL         +2   eeprom
    // ==============   +14   =========
    
    // LVL1          +0  +2    ha_nlink_check_rx
    //   LVL2        +8  +2    nlink_node_on_rx
    //     LVL3      +7  +2    ledlight_on_rx
    //       LVL4    +2  +2    ha_node_ledlight_off
    // ==============   +25    =========
    
    // LVL1          +4  +2    ha_nlink_check_tx
    //   LVL2        +8  +2    nlink_node_on_rx
    //     LVL3      +7  +2    ledlight_on_rx
    //       LVL4    +2  +2    ha_node_ledlight_off
    // ==============   +29    =========
    // ISR ==========   +23
    // Total            +52
    
    // Wait a little just in case
    for(uint8_t uc_i = 0; uc_i < 255U; uc_i++) {

        __asm__ __volatile__ ("    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"
                                ::);
    }

    g_heap[52] = 0x33;
    g_heap[53] = 0x44;

    ha_dev_leds_init();
    ha_nlink_init();

    ll_ambiled = ha_node_ledlight_create(&ambiled_ll_cfg, &ambiled_ll_action[0]);
    sw_ambiled = ha_node_switch_create(&ambiled_sw_cfg);

    sei();

    // Endless loop
    while(1) {
        __asm__ __volatile__ ("    nop\n    nop\n    nop\n    nop\n"\
        "    nop\n    nop\n    nop\n    nop\n"\
        ::);

        ha_node_ledlight_on_idle(ll_ambiled);       // stack +21
        
        ha_nlink_check_rx();                        // stack +25
        ha_nlink_check_tx();                        // stack +29
        if (guc_led_intenisity_timer) {
            guc_led_intenisity_timer = 0;
            ha_node_ledlight_on_timer(ll_ambiled);  // stack +14
        }
        
        if (guc_switches_timer) {
            guc_switches_timer = 0;
            ha_node_switch_on_timer(sw_ambiled);    // stack +7
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
