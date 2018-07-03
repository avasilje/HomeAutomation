/*
 * TODO:
 *    1. Add phts I2C transaction
 *    2. Add result ready callback - will be processed upon transaction completed
 */
#include "ha-phts.h"
#include "ha-i2c.h"

// From PHT sensor data sheet
//	Sensor type						I2C address (binary value)		I2C address (hex. value)
//	Pressure and Temperature P&T	1110110							0x76
//	Relative Humidity RH			1000000							0x40

#define PT_ADDR_RD	((0x76 << 1) + 1)
#define PT_ADDR_WR	((0x76 << 1) + 0)

#define RH_ADDR_RD	((0x40 << 1) + 1)
#define RH_ADDR_WR	((0x40 << 1) + 0)


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

uint32_t temperature = 0;
uint32_t pressure = 0;
uint8_t d1_dbg = PT_CMD_CONVERT_D1_OSR_4096;
uint8_t d2_dbg = PT_CMD_CONVERT_D2_OSR_4096;
#if 0
static void ha_phts_pt_prom_crc(ha_phts_t *sensor)
{
    uint16_t *n_prom = sensor->pt_prom.raw16;

    int cnt; // simple counter
    uint16_t n_rem=0; // crc remainder
    uint8_t n_bit;

    n_prom[0]=((n_prom[0]) & 0x0FFF);           // CRC byte is replaced by 0
    n_prom[7]=0;                                // Subsidiary value, set to 0
    for (cnt = 0; cnt < 16; cnt++)              // operation is performed on bytes
    {                                           // choose LSB or MSB
        if (cnt %2 == 1) {
            n_rem ^= n_prom[cnt>>1] & 0x00FF;
        } else {
            n_rem ^= n_prom[cnt>>1] >>8;
        }

        for (n_bit = 8; n_bit > 0; n_bit--) {
            if (n_rem & 0x8000) {
                n_rem = (n_rem << 1) ^ 0x3000;
            } else {
                n_rem = (n_rem << 1);
            }
        }
    }
    n_rem= (n_rem >> 12) & 0x000F; // final 4-bit remainder is CRC code
    //return (n_rem ^ 0x00);
}
#endif 

void ha_phts_state_init(i2c_trans_t *tr) {
	
	ha_phts_t *sensor = (ha_phts_t *)tr->cb_ctx;

	// TODO: Check transaction error 
	switch (tr->cb_param) {
		case TR_PARAM_PT_RESET:
			ha_i2c_cmd(PT_ADDR_WR, RH_CMD_RESET, ha_phts_state_init, sensor, TR_PARAM_RH_RESET);
			break;

		case TR_PARAM_RH_RESET:
			sensor->prom_rd_idx = 0;
			ha_i2c_read16(RH_ADDR_WR, PT_CMD_PROM_RD, ha_phts_state_init, sensor, TR_PARAM_PT_PROM);
			break;

		case TR_PARAM_PT_PROM:
			sensor->pt_prom.raw8[sensor->prom_rd_idx + 0] = tr->data[3];
			sensor->pt_prom.raw8[sensor->prom_rd_idx + 1] = tr->data[4];
			sensor->prom_rd_idx += 2;

			if (sensor->prom_rd_idx == (HA_PHTS_PT_PROM_SIZE << 1)) {
				// PT PROM read finished - state to IDLE
				sensor->state = PHTS_STATE_IDLE;
				sensor->poll_timer = sensor->idle_timeout;
			} else {
				// Continue PT PROM reading
				ha_i2c_read16(RH_ADDR_WR, PT_CMD_PROM_RD + sensor->prom_rd_idx, ha_phts_state_init, sensor, TR_PARAM_PT_PROM);
			}
			break;

		}
}

void ha_phts_state_measure(i2c_trans_t *tr) {
	
	ha_phts_t *sensor = (ha_phts_t *)tr->cb_ctx;

	// TODO: Check transaction error
	switch (tr->cb_param) {
		case TR_PARAM_PT_D1_START:
			sensor->state = PHTS_STATE_PT_D1_POLL;
			sensor->poll_timer = sensor->d1_poll_timeout;
			break;
		case TR_PARAM_PT_D1_POLL:
			sensor->pressure.raw8[0] = tr->data[3];
			sensor->pressure.raw8[1] = tr->data[4];

			sensor->state = PHTS_STATE_PT_D2_CONVERT;
			sensor->poll_timer = 0;	// start in next poll cycle
			break;

		case TR_PARAM_PT_D2_START:
			sensor->state = PHTS_STATE_PT_D2_POLL;
			sensor->poll_timer = sensor->d2_poll_timeout;
			break;

		case TR_PARAM_PT_D2_POLL:
			sensor->temperature.raw8[0] = tr->data[3];
			sensor->temperature.raw8[1] = tr->data[4];

			sensor->state = PHTS_STATE_IDLE;
			sensor->poll_timer = sensor->idle_timeout;	
			break;

	} // End of state_measurement switch
}

void ha_phts_poll(ha_phts_t *sensor) {

	if (sensor->poll_timer < 0) {
		return;
	}

    if (sensor->poll_timer > 0 ) {
        sensor->poll_timer --;
        return;
    }

	// Disable timer by default
	sensor->poll_timer = -1;

	switch(sensor->state) {
		case PHTS_STATE_INIT:
			ha_i2c_cmd(PT_ADDR_WR, PT_CMD_RESET, ha_phts_state_init, sensor, TR_PARAM_PT_RESET);
			break;

		case PHTS_STATE_IDLE:
			// Initiate measurement
			ha_i2c_cmd(PT_ADDR_WR, PT_CMD_CONVERT_D1_OSR_8192, ha_phts_state_measure, sensor, TR_PARAM_PT_D1_START);
			break;

		case PHTS_STATE_PT_D1_POLL:	
			ha_i2c_read24(RH_ADDR_WR, PT_CMD_ADC_RD, ha_phts_state_measure, sensor, TR_PARAM_PT_D1_POLL);
			break;

		case PHTS_STATE_PT_D2_CONVERT:
			ha_i2c_cmd(PT_ADDR_WR, PT_CMD_CONVERT_D2_OSR_8192, ha_phts_state_measure, sensor, TR_PARAM_PT_D2_START);
			break;

		case PHTS_STATE_PT_D2_POLL:
			ha_i2c_read24(RH_ADDR_WR, PT_CMD_ADC_RD, ha_phts_state_measure, sensor, TR_PARAM_PT_D2_POLL);		// Switch poll back to IDLE
			break;

	}

}

void ha_phts_init(ha_phts_t *sensor) {

	sensor->state = PHTS_STATE_INIT;
	sensor->poll_timer = 0;

}
