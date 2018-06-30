#include "ha-i2c.h"
//#include <avr/interrupt.h>
//#include <avr/pgmspace.h>
//#include <avr/io.h>
//#include <avr/eeprom.h>
//#include <stddef.h>

i2c_t g_i2c;

void ha_i2c_init() {

    // Init I2C

    // Configure IO pins
    I2C_SDA_PORT &= ~_BV(I2C_SDA);
    I2C_SDA_DIR  &= ~_BV(I2C_SDA);		// Ext Pull-up HIGH

    I2C_SCL_PORT &= ~_BV(I2C_SCL);		// Never change it - SCL either forced-down or ext. pulled-up
    I2C_SCL_DIR  &= ~_BV(I2C_SCL);		// Ext Pull-up HIGH

    // NOTE: From PHTS data sheet:
    // The reset can be sent at any time. In the event that there is
    // not a successful power on reset this may be caused by the SDA
    // being blocked by the module in the acknowledge state. The only
    // way to get the ASIC to function is to send several SCLs followed
    // by a reset sequence or to repeat power on reset.
    // AV: Force several START/STOP conditions according to ^^^^
    scl_delay();
	while(bit_is_clear(I2C_SDA_PIN, I2C_SDA)){

        I2C_SCL_DIR |= _BV(I2C_SCL);				// Force SCL low
        scl_delay();

	    I2C_SCL_DIR &= ~_BV(I2C_SCL);				// Release SCL Ext Pull-up
        scl_delay();
    }

    // Configure Control Regs
	// ...
}

void sda_delay()
{
	// Note: the delay must be bigger than time between SCL released and SCL high
	uint8_t i;
    for(i = 0; i < 64; i++){
	    __asm__ __volatile__ (
        "    nop\n    nop\n    nop\n    nop\n"\
        "    nop\n    nop\n    nop\n    nop\n"\
							    ::);
    }

}

void scl_delay()
{
	// Note: the delay must be bigger than time between SCL released and SCL high
	uint8_t i;
	for(i = 0; i < 128; i++){
		__asm__ __volatile__ (
        "    nop\n    nop\n    nop\n    nop\n"\
        "    nop\n    nop\n    nop\n    nop\n"\
		::);
	}

}

/*
 * S - Start bit required
 * B - Data byte (8bits)
 * P - Stop bit required
 * returns ACK
 */
static uint8_t ha_i2c_write_primitive(uint8_t s, uint8_t b, uint8_t p)
{
	uint8_t ack;
	int8_t i;

	/*
	 * Start condition
	 */
	if (s) {
        // Ensure default state
		loop_until_bit_is_set(I2C_SDA_PIN, I2C_SDA);	// Wait SDA released (might be still pulled-down by slave in ack state)
		loop_until_bit_is_set(I2C_SCL_PIN, I2C_SCL);    // Wait SCL released
		scl_delay();								// Delay

		// I2C_SDA_PORT &= ~_BV(I2C_SDA);
	    I2C_SDA_DIR  |= _BV(I2C_SDA);               // Set SDA low (START)
        scl_delay();

		I2C_SCL_DIR |= _BV(I2C_SCL);				// Force SCL low
        scl_delay();
	}

	/*
	 * Data transfer
	 */
	for (i = 0; i < 8; i++)	{

		if (b & 0x80) {
	        I2C_SDA_DIR  &= ~_BV(I2C_SDA);          // Release SDA
		} else {
	        I2C_SDA_DIR  |= _BV(I2C_SDA);           // Set SDA Low
		}
		b <<= 1;
		sda_delay();								// Delay

		I2C_SCL_DIR &= ~_BV(I2C_SCL);				// Release SCL Ext Pull-up
		loop_until_bit_is_set(I2C_SCL_PIN, I2C_SCL);	// Wait SCL high
        scl_delay();

		I2C_SCL_DIR |= _BV(I2C_SCL);				// Force SCL low
        scl_delay();
	}

	/*
	 * ACK
	 */
	I2C_SDA_DIR  &= ~_BV(I2C_SDA);              // SDA Release
    sda_delay();								// Delay

	I2C_SCL_DIR  &= ~_BV(I2C_SCL);				// Release SCL Ext Pull-up
	loop_until_bit_is_set(I2C_SCL_PIN, I2C_SCL);	// Wait SCL high
    scl_delay();

	ack = bit_is_set(I2C_SDA_PIN, I2C_SDA);		// Get SDA

    sda_delay();								// Delay
	I2C_SCL_DIR |= _BV(I2C_SCL);				// Force SCL low
    scl_delay();

	/*
	 * Stop
	 */
	if (p) {
		I2C_SDA_DIR  |= _BV(I2C_SDA);               // Set SDA Low
		sda_delay();								// Delay

		I2C_SCL_DIR  &= ~_BV(I2C_SCL);				// Release SCL Ext Pull-up
        scl_delay();

		I2C_SDA_DIR  &= ~_BV(I2C_SDA);				// Release SDA
        sda_delay();								// Delay
	}
	return ack;
}

/*
 *
 * B - Data byte (8bits)
 * ACK_IN - Master's ACK - 0 forced, 1 - ex. pulled-up. Use 1 if ACK from slave expected
 * P - Stop bit required
 * returns ACK
 */

static uint8_t ha_i2c_read_primitive(uint8_t *b_out, uint8_t ack_in, uint8_t p)
{
	uint8_t ack, b;
	int8_t i;

	/*
	 * Data transfer
	 */
	b = 0;

    // SCL must be forced 0 here
    scl_delay();								// Delay
	for (i = 0; i < 8; i++)	{
		I2C_SCL_DIR &= ~_BV(I2C_SCL);				// Release SCL Ext Pull-up
		loop_until_bit_is_set(I2C_SCL_PIN, I2C_SCL);	// Wait SCL high
		sda_delay();								// Delay

		b <<= 1;
        if (bit_is_set(I2C_SDA_PIN, I2C_SDA)) {
            b |= 1;
        } else {
            b |= 0;		// Get SDA
        }
		scl_delay();								// Delay
        I2C_SCL_DIR |= _BV(I2C_SCL);				// Force SCL low
		scl_delay();								// Delay
	}

	*b_out = b;

	/*
	 * ACK
	 */

	if (ack_in == 0) {
		I2C_SDA_DIR  |= _BV(I2C_SDA);			// Force SDA Low
        sda_delay();  							// Delay
	}

	I2C_SCL_DIR  &= ~_BV(I2C_SCL);				// Release SCL Ext Pull-up
	loop_until_bit_is_set(I2C_SCL_PIN, I2C_SCL);	// Wait SCL high
    scl_delay();

	ack = bit_is_set(I2C_SDA_PIN, I2C_SDA);		// Get SDA

    sda_delay();								// Delay
	I2C_SCL_DIR |= _BV(I2C_SCL);				// Force SCL low
    scl_delay();

	I2C_SDA_DIR  &= ~_BV(I2C_SDA);				// Release SDA
    sda_delay();    							// Delay

	/*
	 * Stop
	 */
	if (p) {
		I2C_SDA_DIR  |= _BV(I2C_SDA);               // Set SDA Low
		sda_delay();								// Delay

		I2C_SCL_DIR  &= ~_BV(I2C_SCL);				// Release SCL Ext Pull-up
        scl_delay();

		I2C_SDA_DIR  &= ~_BV(I2C_SDA);				// Release SDA
        sda_delay();								// Delay
	}
	return ack;
}

uint8_t ha_i2c_write_cmd(uint8_t addr, uint8_t cmd)
{
	uint8_t nack;
	nack = ha_i2c_write_primitive(1, addr, 0);
	if (nack)
		return nack;

    scl_delay();
    scl_delay();
    scl_delay();

	nack = ha_i2c_write_primitive(0, cmd, 1);

    scl_delay();
    scl_delay();
    scl_delay();
	return nack;
}

uint8_t ha_i2c_write_8(uint8_t addr, uint8_t cmd, uint8_t *data)
{
	uint8_t nack;
	nack = ha_i2c_write_primitive(1, addr, 0);
	if (nack)
		return nack;

    scl_delay();
    scl_delay();
    scl_delay();

	nack = ha_i2c_write_primitive(0, cmd, 0);
	if (nack)
		return nack;

    scl_delay();
    scl_delay();
    scl_delay();

	nack = ha_i2c_write_primitive(0, *data, 1);

    scl_delay();
    scl_delay();
    scl_delay();
	return nack;
}

uint8_t ha_i2c_read_8(uint8_t addr, uint8_t ack_in, uint8_t *out)
{
	uint8_t nack = ha_i2c_write_primitive(1, addr, 0);
	if (nack)
		return nack;

	ha_i2c_read_primitive(&out[0], 1, 1);
	return 0;
}

uint8_t ha_i2c_read_16(uint8_t addr, uint8_t ack_in, uint8_t *out)
{
	uint8_t nack = ha_i2c_write_primitive(1, addr, 0);
	if (nack)
		return nack;

	ha_i2c_read_primitive(&out[1], ack_in, 0);
	ha_i2c_read_primitive(&out[0], 1, 1);
	return 0;
}

uint8_t ha_i2c_read_24(uint8_t addr, uint8_t ack_in, uint8_t *out)
{
	uint8_t nack = ha_i2c_write_primitive(1, addr, 0);
	if (nack)
		return nack;

	ha_i2c_read_primitive(&out[2], ack_in, 0);
	ha_i2c_read_primitive(&out[1], ack_in, 0);
	ha_i2c_read_primitive(&out[0], 1, 1);
	return 0;
}
