/*
 * ha_node_switch.h
 *
 * Created: 4/30/2018 4:37:47 PM
 *  Author: Solit
 */

#pragma once
#define SW_EVENT_NONE     0
#define SW_EVENT_ON_OFF   1
#define SW_EVENT_OFF_ON   2
#define SW_EVENT_ON_HOLD  3
#define SW_EVENT_HOLD_OFF 4

extern void ha_node_switch_init();
extern void ha_node_switch_on_timer();
