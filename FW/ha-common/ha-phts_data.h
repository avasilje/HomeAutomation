/*
 * ha-phts-data.h
 *
 */
#pragma once

//#pragma once
//#define PHTS_RD_CNT        0
//#define PHTS_ADC_D2        1     // Temperature
//#define PHTS_ADC_D1        3     // Pressure
//#define PHTS_PT_COEF       5     // Calibration coefficients (7x2)
//#define PHTS_RH           19

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
	uint8_t raw8[2];
	uint16_t s;
} adc_out_t;

typedef struct ha_phts_data_s {

   uint8_t	 read_cnt;		// Free running counter to ensure sensors' data have been updated
   adc_out_t temperature;
   adc_out_t pressure;

} ha_phts_readings_t;

