/*
 * ha_common_xxx.h
 *
 * Created: 28/03/2019 6:57:31 PM
 * Author: solit
 */
#ifdef HA_DEV_FATHER_BATHROOM

#ifndef HA_COMMON_XXX_H_
#define HA_COMMON_XXX_H_

#include <avr/interrupt.h>
#include <avr/io.h>

#define NLINK_NODES_NUM 2       // 1xLEDLIGHT + 1xSwitch
#define NLINK_IO_IDLE_TIMEOUT NLINK_IO_IDLE_TIMEOUT_BATHROOM   // timeout when TX can start to transmit.

#include "ha-dev-base.h"

#endif /* HA_COMMON_XXX_H_ */
#endif /* HA_DEV_FATHER_BATHROOM */

