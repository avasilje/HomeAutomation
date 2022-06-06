/*
 * ha_common.h
 *
 * Created: 5/1/2018 11:39:01 PM
 *  Author: Solit
 */

#pragma once

#if defined(HA_DEV_LOUNGE_BLCN) || defined(HA_DEV_ELBOX) || defined(HA_DEV_ENTRANCE)
#include "ha-common-lounge.h"
#endif 

#if defined(HA_DEV_FATHER_CEIL) || defined(HA_DEV_FATHER_BLCN) || defined (HA_DEV_FATHER_AMBILED)
//#include "ha-common-father.h"
#endif

extern uint16_t gus_trap_line;
extern void FATAL_TRAP (uint16_t us_line_num);

#define INT_FATAL_TRAP(us_line_num)     \
    do {                                \
        gus_trap_line = us_line_num;    \
        while(1);                       \
    } while(1)

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define UNREFERENCED_PARAM(x) x=x

#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1]
