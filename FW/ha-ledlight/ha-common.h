/*
 * ha_common.h
 *
 * Created: 5/1/2018 11:39:01 PM
 *  Author: Solit
 */ 

#pragma once
extern uint16_t gus_trap_line;
extern void FATAL_TRAP (uint16_t us_line_num);

#define INT_FATAL_TRAP(us_line_num)     \
    do {                                \
        gus_trap_line = us_line_num;    \
        while(1);                       \
    } while(1)

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define UNREFERENCED_PARAM(x) x=x