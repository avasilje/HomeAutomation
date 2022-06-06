/*
 * ha_common_xxx.h
 *
 * Created: 27/12/2021
 * Author: solit
 */
#ifdef HA_DEV_FATHER_BLCN

#ifndef HA_COMMON_XXX_H_
#define HA_COMMON_XXX_H_

#include <avr/interrupt.h>
#include <avr/io.h>

#define NLINK_NODES_NUM 2       // 1xLEDLIGHT + ROLL
#define NLINK_IO_IDLE_TIMEOUT NLINK_IO_IDLE_TIMEOUT_BLCN   // timeout when TX can start to transmit.

#include "ha-dev-base.h"

#endif /* HA_COMMON_XXX_H_ */
#endif /* HA_DEV_FATHER_BLCN */

