/*
 * TODO:
 *    1. Add phts I2C transaction
 *    2. Add result ready callback - will be processed upon transaction completed
 */
#include <stdlib.h>
#include <string.h>
#include "ha-phts.h"
#include "ha-i2c.h"

// From PHT sensor data sheet
//	Sensor type						I2C address (binary value)		I2C address (hex. value)
//	Pressure and Temperature P&T	1110110x						0x76
//	Relative Humidity RH			1000000x						0x40

#define PT_ADDR	((0x76 << 1) + 0)

#define RH_ADDR	((0x40 << 1) + 0)

#define PT_CMD_RESET				0x1E
#define PT_CMD_CONVERT_D1_OSR_256	0x40
#define PT_CMD_CONVERT_D1_OSR_512	0x42
#define PT_CMD_CONVERT_D1_OSR_1024	0x44
#define PT_CMD_CONVERT_D1_OSR_2048	0x46
#define PT_CMD_CONVERT_D1_OSR_4096	0x48
#define PT_CMD_CONVERT_D1_OSR_8192	0x4A
#define PT_CMD_CONVERT_D2_OSR_256	0x50
#define PT_CMD_CONVERT_D2_OSR_512	0x52
#define PT_CMD_CONVERT_D2_OSR_1024	0x54
#define PT_CMD_CONVERT_D2_OSR_2048	0x56
#define PT_CMD_CONVERT_D2_OSR_4096	0x58
#define PT_CMD_CONVERT_D2_OSR_8192	0x5A
#define PT_CMD_ADC_RD				0x00
#define PT_CMD_PROM_RD				0xA0

#define  RH_CMD_RESET			0xFE
#define  RH_CMD_USER_REG_WR		0xE6
#define  RH_CMD_USER_REG_RD		0xE7
#define  RH_CMD_MEASURE_HOLD	0xE5
#define  RH_CMD_MEASURE_NOHOLD	0xF5
#define  RH_CMD_PROM_RD			0xA0

void _phts_on_measure(i2c_trans_t *tr)
{
	ha_phts_t *phts = (ha_phts_t *)tr->cb_ctx;

	// TODO: Check transaction error
	switch (tr->cb_param) {
		case TR_PARAM_PT_D1_START:
            phts->readings.read_cnt ++;
			phts->state = PHTS_STATE_PT_D1_POLL;
			phts->poll_timer = phts->d1_poll_timeout;
			break;
		case TR_PARAM_PT_D1_POLL:
			phts->readings.pressure.raw8[0] = tr->data[4];
			phts->readings.pressure.raw8[1] = tr->data[3];

			phts->state = PHTS_STATE_PT_D2_CONVERT;
			phts->poll_timer = 0;	// start in next poll cycle
			break;

		case TR_PARAM_PT_D2_START:
			phts->state = PHTS_STATE_PT_D2_POLL;
			phts->poll_timer = phts->d2_poll_timeout;
			break;

		case TR_PARAM_PT_D2_POLL:
			phts->readings.temperature.raw8[0] = tr->data[4];
			phts->readings.temperature.raw8[1] = tr->data[3];

			phts->state = PHTS_STATE_IDLE;
			phts->poll_timer = phts->idle_timeout;
			break;

	} // End of state_measurement switch
}

uint8_t ha_phts_measurement_poll(ha_phts_t *phts) {

    int8_t rc = 0;

	if (phts->poll_timer < 0) {
		return rc;
	}

    if (phts->poll_timer > 0 ) {
        phts->poll_timer --;
        return rc;
    }

	// Disable timer by default
	phts->poll_timer = -1;

	switch(phts->state) {
		case PHTS_STATE_IDLE:
			// Initiate measurement
			rc = ha_i2c_cmd(PT_ADDR, PT_CMD_CONVERT_D1_OSR_8192, _phts_on_measure, phts, TR_PARAM_PT_D1_START);
			break;

		case PHTS_STATE_PT_D1_POLL:
			rc = ha_i2c_read24(PT_ADDR, PT_CMD_ADC_RD, _phts_on_measure, phts, TR_PARAM_PT_D1_POLL);
			break;

		case PHTS_STATE_PT_D2_CONVERT:
			rc = ha_i2c_cmd(PT_ADDR, PT_CMD_CONVERT_D2_OSR_8192, _phts_on_measure, phts, TR_PARAM_PT_D2_START);
			break;

		case PHTS_STATE_PT_D2_POLL:
			rc = ha_i2c_read24(PT_ADDR, PT_CMD_ADC_RD, _phts_on_measure, phts, TR_PARAM_PT_D2_POLL);		// Switch poll back to IDLE
			break;
	} // End of state Switch

    if (rc < 0) {
        phts->poll_timer = phts->idle_timeout;  // I2C not ready - repeat later
    }
    return rc;
}


void _phts_on_pt_reset(i2c_trans_t *tr)
{
	ha_phts_t *sensor = (ha_phts_t *)tr->cb_ctx;
	sensor->state = PHTS_STATE_RESET_PT;
}

void _phts_on_rh_reset(i2c_trans_t *tr)
{
	ha_phts_t *phts = (ha_phts_t *)tr->cb_ctx;
	phts->state = PHTS_STATE_RESET_RH;
}

void _phts_on_prom_rd(i2c_trans_t *tr)
{
	ha_phts_t *phts = (ha_phts_t *)tr->cb_ctx;

   	phts->state = PHTS_STATE_PROM_RD;
    phts->prom_rd_idx = tr->data[1] & 0x0F;
    phts->pt_prom.raw8[phts->prom_rd_idx + 1] = tr->data[3];
    phts->pt_prom.raw8[phts->prom_rd_idx + 0] = tr->data[4];
}

uint8_t ha_phts_reset_pt(ha_phts_t *phts) {
    return ha_i2c_cmd(PT_ADDR, PT_CMD_RESET, _phts_on_pt_reset, phts, 0);
}

uint8_t ha_phts_reset_rh(ha_phts_t *phts) {
    return ha_i2c_cmd(RH_ADDR, RH_CMD_RESET, _phts_on_rh_reset, phts, 0);
}

uint8_t ha_phts_prom_rd(ha_phts_t *phts, uint8_t rd_idx) {
    return ha_i2c_read16(PT_ADDR, PT_CMD_PROM_RD + rd_idx, _phts_on_prom_rd, phts, 0);
}

uint8_t ha_phts_measurement_start(ha_phts_t *phts)
{
    phts->state = PHTS_STATE_IDLE;
    phts->poll_timer = 0;
    return 0;
}

void ha_phts_init(ha_phts_t *phts)
{
    memset((uint8_t*)phts, 0, sizeof(ha_phts_t));
    phts->state = PHTS_STATE_INIT;
}

