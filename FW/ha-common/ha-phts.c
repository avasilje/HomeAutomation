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

static uint8_t ha_phts_pt_get(uint8_t *adc_out)
{
	uint8_t nack = ha_i2c_write_cmd(PT_ADDR_WR, PT_CMD_ADC_RD);
	if (nack) return nack;
	
	ha_i2c_read_24(PT_ADDR_WR, 1, adc_out);
	return 0;
}

static void ha_phts_pt_convert_d1()
{
	ha_i2c_write_cmd(PT_ADDR_WR, PT_CMD_CONVERT_D1_OSR_8192);
}
static void ha_phts_pt_convert_d2()
{
	ha_i2c_write_cmd(PT_ADDR_WR, PT_CMD_CONVERT_D2_OSR_8192);
}

void ha_phts_poll(phts_t *sensor) {
	
    if (sensor->poll_timer > 0 ) {
        sensor->poll_timer --;
        return;
    }

	switch(sensor->state) {
		case PHTS_STATE_IDLE:
			ha_phts_pt_convert_d1();
			sensor->state = PHTS_STATE_POLL_PT_D1;
			sensor->poll_timer = sensor->d1_poll_timeout;
			break;
		case PHTS_STATE_POLL_PT_D1:
			if (ha_phts_pt_get(sensor->temperature.raw)) {
				sensor->poll_timer = sensor->d1_poll_timeout;
				break;	// ADC result not ready yet
			}
			ha_phts_pt_convert_d2();
			sensor->state = PHTS_STATE_POLL_PT_D2;
			sensor->poll_timer = PHTS_STATE_POLL_PT_D2;
			break;
		case PHTS_STATE_POLL_PT_D2:
			if (ha_phts_pt_get(sensor->pressure.raw)) {
				sensor->poll_timer = sensor->d2_poll_timeout;
				break;	// ADC result not ready yet
			}
			sensor->read_cnt ++;
			sensor->state = PHTS_STATE_IDLE;
			sensor->poll_timer = sensor->idle_timeout;
			break;
	}

}

/*
 * PHT sensor reset
 * Should be executed once, just after power up
 */ 
void ha_phts_reset() 
{
	ha_i2c_write_cmd(PT_ADDR_WR, PT_CMD_RESET);
	ha_i2c_write_cmd(RH_ADDR_WR, RH_CMD_RESET);
}

/*
 * Read pressure & temperature (PT) calibration values
 * Should be executed once, just after reset
 */ 
void ha_phts_prom_read_pt(phts_t *sensor)
{
	int8_t i;
	uint8_t *data_ptr = sensor->pt_prom.raw;
	uint8_t addr = PT_CMD_PROM_RD;
	
	for (i = 0; i < HA_PHTS_PT_PROM_SIZE; i++) {
		ha_i2c_write_cmd(PT_ADDR_WR, addr);
		ha_i2c_read_16(PT_ADDR_WR, 0, data_ptr);
		addr += 2;
		data_ptr += 2; 
	}
}

/*
 * Read relative humidity (RH) calibration values
 * Should be executed once, just after reset
 */ 
void ha_phts_prom_read_rh() 
{
	// Not in use
}

void ha_phts_init(phts_t *sensor) {

	ha_phts_reset();
    ha_phts_prom_read_pt(sensor);
}
