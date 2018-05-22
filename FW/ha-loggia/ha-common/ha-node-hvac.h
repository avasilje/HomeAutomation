/*
 * ha_node_hvac.h
 *
 * Created: 13/05/2018 4:37:47 PM
 *  Author: Solit
 */

#pragma once
#define HVAC_DATA_STATE_CURR_MASK 0x07
#define HVAC_DATA_STATE_STMR_MASK 0x08
#define HVAC_DATA_STATE_TRGT_MASK 0x70
#define HVAC_DATA_STATE_ALRM_MASK 0x80

#define HVAC_DATA_STATE 0
#define HVAC_DATA_HEATER_MVAL 1
#define HVAC_DATA_HEATER_CURR 2
#define HVAC_DATA_LAST 3

#define HVAC_STATE_S1 1
#define HVAC_STATE_S2 2
#define HVAC_STATE_S3 3
#define HVAC_STATE_S4 4

#define HVAC_HEATER_CTRL_MODE_MASK  (1 << 7)
#define HVAC_HEATER_CTRL_MODE_CONST 0                           // Heater control set directly by user
#define HVAC_HEATER_CTRL_MODE_TEMP  HVAC_HEATER_CTRL_MODE_MASK  // Auto regulation as function of in/out temperature
#define HVAC_HEATER_CTRL_VAL_MASK   ((1<< 7) - 1)
