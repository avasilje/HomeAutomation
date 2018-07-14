/*
 * ha_node_phts.h
 *
 */

#pragma once

#define PHTS_CMD_OFF                0       // Contains any of  PHTS_CMD_xxx
#define PHTS_CMD_PROM_RD_IDX_OFF    1

#define PHTS_CMD_RESET_PT       1
#define PHTS_CMD_RESET_RH       2
#define PHTS_CMD_PROM_RD        3
#define PHTS_CMD_MEASURE        4


#define PHTS_STATE         0
#define PHTS_READINGS      1    // PHTS_STATE == IDLE
#define PHTS_PROM_RD_IDX   1    // PHTS_STATE == PROM_RD
#define PHTS_PROM_COEFF    2

#include "ha-phts_data.h"

extern void ha_node_phts_init();
extern void ha_node_phts_on_timer();
extern void ha_node_phts_on_idle();
