/*
 * ha_common_xxx.h
 *
 * Created: 27/12/2021
 * Author: solit
 */
#ifdef HA_DEV_LOUNGE_BLCN

#pragma once

#include <avr/interrupt.h>
#include <avr/io.h>

#define NLINK_NODES_NUM 3       // 1xLEDLIGHT + 2xROLL
#define NLINK_IO_IDLE_TIMEOUT NLINK_IO_IDLE_TIMEOUT_LBLCN   // timeout when TX can start to transmit.

#include "ha-dev-base.h"

#endif /* HA_DEV_LOUNGE_BLCN */

