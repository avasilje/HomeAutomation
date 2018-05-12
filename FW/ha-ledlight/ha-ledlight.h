#pragma once
/*
 * ha_ledlight.h
 *
 * Created: 5/6/2018 12:36:34 AM
 *  Author: solit
 */


#define LEDS_NUM            4
#define LED_MODES_NUM       3

#define SWITCHES_NUM        3
#define SWITCH_MODES_NUM    2

#define LED_MODE_X3            0
#define LED_MODE_ON            2
#define LED_MODE_OFF           3
#define LED_MODE_ON_TRANS      4
#define LED_MODE_OFF_TRANS     5

#define LED_FADEIN_STEP_PERIOD     1     // in 10ms ticks
#define LED_FADEOUT_STEP_PERIOD    1     // in 10ms ticks

// INFO structure for particular light source
typedef struct {
    uint8_t  uc_current_intensity_idx;
    uint8_t  uc_target_intensity_idx;
    uint8_t  uc_fade_timer;
} LED_INFO;

#define IN_SW_0_MASK (1 << PINB7)
#define IN_SW_1_MASK (1 << PINB6)
#define IN_SW_2_MASK (1 << PINB0)
#define IN_SW_3_MASK (1 << PINB1)

#define IN_SW_0 PINB7
#define IN_SW_1 PINB6
#define IN_SW_2 PINB0
#define IN_SW_3 PINB1
#define IN_SW_MASK \
    ((1 << IN_SW_0)|\
     (1 << IN_SW_1)|\
     (1 << IN_SW_2)|\
     (1 << IN_SW_3))

#define OUT_SW0_0 PINB4
#define OUT_SW0_1 PINB3
#define OUT_SW0_2 PINB2

#define OUT_SW0_0_MASK (1 << OUT_SW0_0)
#define OUT_SW0_1_MASK (1 << OUT_SW0_1)
#define OUT_SW0_2_MASK (1 << OUT_SW0_2)

#define OUT_SW0_MASK \
    ((1 << OUT_SW0_0)|\
     (1 << OUT_SW0_1)|\
     (1 << OUT_SW0_2))

#define OUT_SW1_3    PIND5
#define OUT_SW1_3_MASK (1 << PIND5)
#define OUT_SW1_MASK (OUT_SW1_3_MASK)

#define ENABLE_FAST_PWM2 \
    TCCR2 = (1 << WGM20) |\
            (1 << WGM21) |\
            (1 << COM20) |\
            (1 << COM21) |\
            (1 << CS20 )

