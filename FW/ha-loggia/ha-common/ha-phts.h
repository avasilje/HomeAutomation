#pragma once
#include <stdint.h>
#include "ha-phts_data.h"

// MS860702BA01 - PHT Combination Sensor
#define PHTS_STATE_INIT				1
#define PHTS_STATE_RESET_PT         2       // State controlled by user (CtrlCon)
#define PHTS_STATE_RESET_RH         3       // State controlled by user (CtrlCon)
#define PHTS_STATE_PROM_RD          4       // State controlled by user (CtrlCon)

#define PHTS_STATE_IDLE				5
#define PHTS_STATE_PT_D1_CONVERT	6
#define PHTS_STATE_PT_D1_POLL		7
#define PHTS_STATE_PT_D2_CONVERT	8
#define PHTS_STATE_PT_D2_POLL		9

#define TR_PARAM_PT_RESET 2
#define TR_PARAM_RH_RESET 3
#define TR_PARAM_PT_PROM  4
#define TR_PARAM_RH_PROM  5
#define TR_PARAM_PT_D1_START 6
#define TR_PARAM_PT_D1_POLL  7
#define TR_PARAM_PT_D2_START 8
#define TR_PARAM_PT_D2_POLL  9

typedef struct phts_s {
   int8_t poll_timer;
   int8_t state;
   int8_t prom_rd_idx;

   // Timeouts configured once by a parent module before ha_phts_init()
   uint8_t d1_poll_timeout;
   uint8_t d2_poll_timeout;
   uint8_t idle_timeout;

   ha_phts_readings_t readings;

   union pt_prom_u pt_prom;     // Q: ?Not a part of phts because phts don't care about real temperature, but about adc readings only?

} ha_phts_t;

uint8_t ha_phts_reset_pt(ha_phts_t *sensor);
uint8_t ha_phts_reset_rh(ha_phts_t *sensor);
uint8_t ha_phts_prom_rd(ha_phts_t *sensor, uint8_t rd_idx);
uint8_t ha_phts_measurement_start(ha_phts_t *sensor);
uint8_t ha_phts_measurement_poll(ha_phts_t *sensor);

void ha_phts_init(ha_phts_t *sensor);
