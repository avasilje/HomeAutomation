#pragma once
/*
 * ha_mainlight.h
 *
 * Created: 17/03/2018 12:36:34 AM
 *  Author: solit
 */

#define LEDS_NUM            4
#define SWITCHES_NUM        3

#define SWITCH_MODES_NUM    2

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

#define OUT_LED0_0 PINB2
#define OUT_LED0_1 PINB3
#define OUT_LED0_2 PINB4

#define OUT_LED0_0_MASK (1 << OUT_LED0_0)
#define OUT_LED0_1_MASK (1 << OUT_LED0_1)
#define OUT_LED0_2_MASK (1 << OUT_LED0_2)

#define OUT_LED0_MASK \
    ((1 << OUT_LED0_0)|\
     (1 << OUT_LED0_1)|\
     (1 << OUT_LED0_2))

#define OUT_LED1_3    PIND5
#define OUT_LED1_3_MASK (1 << PIND5)
#define OUT_LED1_MASK (OUT_LED1_3_MASK)

#define ENABLE_FAST_PWM2 \
    TCCR2 = (1 << WGM20) |\
            (1 << WGM21) |\
            (1 << COM20) |\
            (1 << COM21) |\
            (1 << CS20 )

