#pragma once
#include <stdint.h>

// MS860702BA01 - PHT Combination Sensor

#define HA_PHTS_PT_PROM_SIZE	0x07	// in 16bit words
#define HA_PHTS_RH_PROM_SIZE	0x07	// in 16bit words

typedef struct pt_coef_s {
	uint16_t crc_factory_def;				// 4bit CRC + factory defined data
	uint16_t c1_pressure_sensitivity;		// Pressure sensitivity | SENS T1
	uint16_t c2_pressure_offset;			// Pressure offset | OFF T1
	uint16_t c3_pressure_sensitivity_tcoef;	// Temperature coefficient of pressure sensitivity | TCS
	uint16_t c4_pressure_offset_tcoef;		// Temperature coefficient of pressure offset | TCO
	uint16_t c5_ref_temperature;			// Reference temperature | T REF
	uint16_t c6_temperature_tcoef;			// Temperature coefficient of the temperature | TEMPSENS
} pt_coef_t;

union pt_prom_u {
	uint8_t raw8[HA_PHTS_PT_PROM_SIZE << 1];
	uint16_t raw16[HA_PHTS_PT_PROM_SIZE + 1];   
	pt_coef_t coef;
};

typedef union adc_out_u {
	uint8_t raw[4];
	uint16_t s[2];
} adc_out_t;

#define PHTS_STATE_NONE			0
#define PHTS_STATE_IDLE			1
#define PHTS_STATE_POLL_PT_D1	2
#define PHTS_STATE_POLL_PT_D2	3

typedef struct phts_s {
   int8_t poll_timer;
   int8_t state;
   
   // Timeouts configured once by a parent module before ha_phts_init()
   uint8_t d1_poll_timeout;
   uint8_t d2_poll_timeout;
   uint8_t idle_timeout;
   
   union pt_prom_u pt_prom;
   adc_out_t temperature;
   adc_out_t pressure;
   uint8_t	 read_cnt;		// Free running counter to ensure sensors' data have been updated
} ha_phts_t;

void ha_phts_poll(ha_phts_t *sensor);
void ha_phts_init(ha_phts_t *sensor);

