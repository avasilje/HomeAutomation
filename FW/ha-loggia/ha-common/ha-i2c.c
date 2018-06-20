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
	
    // Configure Control Regs
	// ...
}

void sda_delay()
{
	// Note: the delay must be bigger than time between SCL released and SCL high
	uint8_t i;
    for(i = 0; i < 32; i++){
	    __asm__ __volatile__ ("    nop\n    nop\n    nop\n    nop\n"\
							    ::);
    }

}

void scl_delay()
{
	// Note: the delay must be bigger than time between SCL released and SCL high
	uint8_t i;
	for(i = 0; i < 128; i++){
		__asm__ __volatile__ ("    nop\n    nop\n    nop\n    nop\n"\
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
		while(bit_is_set(I2C_SDA_PIN, I2C_SCL));	// Wait SDA released (might be still pulled-down by slave in ack state)
		sda_delay();								// Delay
				
		I2C_SCL_DIR &= ~_BV(I2C_SCL);				// Release SCL
		while(bit_is_set(I2C_SCL_PIN, I2C_SCL));
		sda_delay();								// Delay
		
		I2C_SDA_PORT &= ~_BV(I2C_SDA);	// Set SDA low
	    I2C_SDA_DIR  |= _BV(I2C_SDA);
		sda_delay();					// Delay
	}
	
	/*
	 * Data transfer
	 */
	I2C_SDA_DIR  |= _BV(I2C_SDA);
	for (i = 0; i < 8; i++)	{
		I2C_SCL_DIR |= _BV(I2C_SCL);				// Force SCL low

		sda_delay();								// Delay
		if (b & 0x80) {
			I2C_SDA_PORT |= _BV(I2C_SDA);			// Set SDA High
		} else {
			I2C_SDA_PORT &= ~_BV(I2C_SDA);			// Set SDA Low
		}
		b >>= 1;

		I2C_SCL_DIR &= ~_BV(I2C_SCL);				// Release SCL Ext Pull-up
		while(bit_is_set(I2C_SCL_PIN, I2C_SCL));	// Wait SCL high
		sda_delay();								// Delay
	}

	/*
	 * ACK
	 */
	I2C_SDA_DIR  &= ~_BV(I2C_SDA);
	I2C_SDA_PORT &= ~_BV(I2C_SDA);				// Release SDA

	I2C_SCL_DIR  |=  _BV(I2C_SCL);				// Force SCL low

	scl_delay();								// Delay
	I2C_SCL_DIR  &= ~_BV(I2C_SCL);				// Release SCL Ext Pull-up

	while(bit_is_set(I2C_SCL_PIN, I2C_SCL));	// Wait SCL high

	sda_delay();								// Delay
	ack = bit_is_set(I2C_SDA_PIN, I2C_SDA);		// Get SDA

	/*
	 * Stop
	 */
	if (p) {
		I2C_SCL_DIR  |=  _BV(I2C_SCL);				// Force SCL low

		sda_delay();								// Delay
		I2C_SDA_DIR  |= _BV(I2C_SDA);
		I2C_SDA_PORT &= ~_BV(I2C_SDA);				// Set SDA Low
		
		I2C_SCL_DIR  &= ~_BV(I2C_SCL);				// Release SCL Ext Pull-up

		sda_delay();								// Delay
		I2C_SDA_DIR  &= ~_BV(I2C_SDA);				// Release SDA 
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

	for (i = 0; i < 8; i++)	{
		I2C_SCL_DIR |= _BV(I2C_SCL);				// Force SCL low

		scl_delay();								// Delay
		I2C_SCL_DIR &= ~_BV(I2C_SCL);				// Release SCL Ext Pull-up
		while(bit_is_set(I2C_SCL_PIN, I2C_SCL));	// Wait SCL high

		sda_delay();								// Delay
		b |= bit_is_set(I2C_SDA_PIN, I2C_SDA);		// Get SDA
		b <<= 1;
	}

	*b_out = b;
	
	/*
	 * ACK
	 */
	if (ack_in == 0) {
		I2C_SDA_DIR  |= _BV(I2C_SDA);			// Force SDA Low
	}
	
	I2C_SCL_DIR  |=  _BV(I2C_SCL);				// Force SCL low

	scl_delay();								// Delay
	I2C_SCL_DIR  &= ~_BV(I2C_SCL);				// Release SCL Ext Pull-up

	while(bit_is_set(I2C_SCL_PIN, I2C_SCL));	// Wait SCL high

	sda_delay();								// Delay
	ack = bit_is_set(I2C_SDA_PIN, I2C_SDA);		// Get SDA
	I2C_SDA_DIR  &= ~_BV(I2C_SDA);				// Release SDA

	/*
	 * Stop
	 */
	if (p) {
		I2C_SCL_DIR  |=  _BV(I2C_SCL);				// Force SCL low

		sda_delay();								// Delay
		I2C_SDA_DIR  |= _BV(I2C_SDA);
		I2C_SDA_PORT &= ~_BV(I2C_SDA);				// Set SDA Low
		
		I2C_SCL_DIR  &= ~_BV(I2C_SCL);				// Release SCL Ext Pull-up

		sda_delay();								// Delay
		I2C_SDA_DIR  &= ~_BV(I2C_SDA);				// Release SDA 
	}
	return ack;
}

uint8_t ha_i2c_write_cmd(uint8_t addr, uint8_t cmd)
{
	uint8_t nack;
	nack = ha_i2c_write_primitive(1, addr, 0);
	if (nack) 
		return nack;
	
	nack = ha_i2c_write_primitive(0, cmd, 1);
	return nack;
}

uint8_t ha_i2c_read_16(uint8_t addr, uint8_t ack_in, uint8_t *out)
{
	uint8_t nack = ha_i2c_write_primitive(1, addr, 0);
	if (nack) 
		return nack;
	
	ha_i2c_read_primitive(out++, ack_in, 0);
	ha_i2c_read_primitive(out, 1, 1);
	return 0;
}

uint8_t ha_i2c_read_24(uint8_t addr, uint8_t ack_in, uint8_t *out)
{
	uint8_t nack = ha_i2c_write_primitive(1, addr, 0);
	if (nack) 
		return nack;
	
	ha_i2c_read_primitive(out++, ack_in, 0);
	ha_i2c_read_primitive(out++, ack_in, 0);
	ha_i2c_read_primitive(out, 1, 1);
	return 0;
}
