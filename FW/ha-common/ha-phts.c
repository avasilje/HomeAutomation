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

static uint8_t ha_phts_rh_user_get(uint8_t *user_out)
{
	uint8_t nack = ha_i2c_write_cmd(RH_ADDR_WR, RH_CMD_USER_REG_RD);
	if (nack)
        return nack;

	ha_i2c_read_8(RH_ADDR_RD, 1, user_out);
	return 0;
}

static uint8_t ha_phts_rh_user_set(uint8_t *user_in)
{
	//uint8_t nack = ha_i2c_write_cmd(RH_ADDR_WR, RH_CMD_USER_REG_WR);
	//if (nack)
        //return nack;

	ha_i2c_write_8(RH_ADDR_WR, RH_CMD_USER_REG_WR, user_in);
	return 0;
}


static uint8_t ha_phts_pt_get(uint8_t *adc_out)
{
	uint8_t nack = ha_i2c_write_cmd(PT_ADDR_WR, PT_CMD_ADC_RD);
	if (nack)
        return nack;

	ha_i2c_read_24(PT_ADDR_RD, 0, adc_out);
	return 0;
}

static void ha_phts_pt_convert_d1()
{
	ha_i2c_write_cmd(PT_ADDR_WR, d1_dbg);
}
static void ha_phts_pt_convert_d2()
{
	ha_i2c_write_cmd(PT_ADDR_WR, d2_dbg);
}

void ha_phts_poll(ha_phts_t *sensor) {

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
static void sec_delay()
{
	// Note: the delay must be bigger than time between SCL released and SCL high
	uint8_t i, j, k;
    for (k = 0; k < 10; k++) {      // 35 => 1sec;
        for (j = 0; j < 80; j ++) {
	        for(i = 0; i < 128; i++){
		        __asm__ __volatile__ (
                "    nop\n    nop\n    nop\n    nop\n"\
                "    nop\n    nop\n    nop\n    nop\n"\
                "    nop\n    nop\n    nop\n    nop\n"\
		        ::);
	        }
        }
    }

}

static void rst_delay()
{
	// Note: the delay must be bigger than time between SCL released and SCL high
	uint8_t i, j;
    for (j = 0; j < 80; j ++) {
	    for(i = 0; i < 128; i++){
		    __asm__ __volatile__ (
            "    nop\n    nop\n    nop\n    nop\n"\
            "    nop\n    nop\n    nop\n    nop\n"\
            "    nop\n    nop\n    nop\n    nop\n"\
		    ::);
	    }
    }

}

void ha_phts_reset()
{
	ha_i2c_write_cmd(PT_ADDR_WR, PT_CMD_RESET);
    rst_delay();
	ha_i2c_write_cmd(RH_ADDR_WR, RH_CMD_RESET);
    rst_delay();
}

/*
 * Read pressure & temperature (PT) calibration values
 * Should be executed once, just after reset
 */
void ha_phts_prom_read_pt(ha_phts_t *sensor)
{
	int8_t i;
	uint8_t *data_ptr = sensor->pt_prom.raw8;
	uint8_t addr = PT_CMD_PROM_RD;

	for (i = 0; i < HA_PHTS_PT_PROM_SIZE; i++) {
		ha_i2c_write_cmd(PT_ADDR_WR, addr);
		ha_i2c_read_16(PT_ADDR_RD, 0, data_ptr);
		addr += 2;
		data_ptr += 2;
	}
    ha_phts_pt_prom_crc(sensor);
}

/*
 * Read relative humidity (RH) calibration values
 * Should be executed once, just after reset
 */
void ha_phts_prom_read_rh()
{
	// Not in use
}

void ha_phts_init(ha_phts_t *sensor) {

    uint8_t user_out, user_in;
	ha_phts_reset();
    uint16_t prom;
    uint8_t prom_addr = PT_CMD_PROM_RD;

    //ha_phts_rh_user_get(&user_out);
    //ha_phts_rh_user_set(&user_in);
    rst_delay();

    ha_i2c_write_cmd(PT_ADDR_WR, prom_addr);
    rst_delay();
	ha_i2c_read_16(PT_ADDR_RD, 0, &prom);
    rst_delay();

    ha_i2c_write_cmd(PT_ADDR_WR, prom_addr);
    rst_delay();
	ha_i2c_read_16(PT_ADDR_RD, 0, &prom);
    rst_delay();

    ha_i2c_write_cmd(PT_ADDR_WR, prom_addr);
    rst_delay();
	ha_i2c_read_16(PT_ADDR_RD, 0, &prom);
    rst_delay();

    ha_i2c_write_cmd(PT_ADDR_WR, prom_addr);
    rst_delay();
	ha_i2c_read_16(PT_ADDR_RD, 0, &prom);
    rst_delay();

    ha_phts_prom_read_pt(sensor);

    //pressure = 0;
    //ha_phts_pt_convert_d2();
    //rst_delay();
    //rst_delay();
    //ha_phts_pt_get((uint8_t*)&pressure);
    //rst_delay();

    while(temperature != 111) {
        ha_phts_pt_convert_d2();
        rst_delay();
        ha_phts_pt_get((uint8_t*)&temperature);
        sec_delay();
    }

    temperature = 0;
    ha_phts_pt_convert_d1();
    rst_delay();
    rst_delay();
    ha_phts_pt_get((uint8_t*)&temperature);
    rst_delay();

    temperature = 0;
    ha_phts_pt_convert_d1();
    rst_delay();
    rst_delay();
    ha_phts_pt_get((uint8_t*)&temperature);
    rst_delay();

    temperature = 0;
    ha_phts_pt_convert_d2();
    rst_delay();
    rst_delay();
    ha_phts_pt_get((uint8_t*)&pressure);
    rst_delay();

}
