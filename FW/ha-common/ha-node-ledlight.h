/*
 * ha_node_ledlight.h
 *
 * Created: 4/30/2018 4:37:47 PM
 *  Author: Solit
 */

#pragma once
#define LL_DATA_MODE        0
#define LL_DATA_DIS_MASK    1
#define LL_DATA_INTENSITY0  2
#define LL_DATA_INTENSITY1  3
#define LL_DATA_INTENSITY2  4
#define LL_DATA_INTENSITY3  5
#define LL_DATA_LEN         6

enum LED_MODE {
    LED_MODE_X3 = 0,
    LED_MODE_ON = 2,
    LED_MODE_OFF = 3,
    LED_MODE_ON_TRANS = 4,
    LED_MODE_OFF_TRANS = 5
};

#define LED_FADEIN_STEP_PERIOD     1     // in timer ticks
#define LED_FADEOUT_STEP_PERIOD    1     // in timer ticks

// INFO structure for particular light source
typedef struct {
    uint8_t  uc_current_intensity_idx;
    uint8_t  uc_target_intensity_idx;
    uint8_t  uc_on_intensity_idx;
    uint8_t  uc_fade_timer;
} LED_INFO;

extern void ha_node_ledlight_init();
extern void ha_node_ledlight_off();
extern void ha_node_ledlight_on();


