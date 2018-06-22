/*
 * ha_common.h
 *
 * Created: 5/1/2018 11:39:01 PM
 *  Author: Solit
 */

#pragma once

// Att!: IDLE TIMEOUT must be unique for a particular device
#define NLINK_IO_IDLE_TIMEOUT_ELBOX     10
#define NLINK_IO_IDLE_TIMEOUT_ENTRANCE  12
#define NLINK_IO_IDLE_TIMEOUT_LOGGIA    14

// Device specific part. Proper device selected by preprocessor defines
#include "ha-common-elbox.h"
#include "ha-common-entrance.h"
#include "ha-common-loggia.h"

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