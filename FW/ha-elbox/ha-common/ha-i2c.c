/*
 * TODO:
 *   1. Add REPEATED START condition
 *   2. Add transaction interrupt on NACK
 *   3. Add call back processing on result ready
 */
#include "ha-i2c.h"

ha_i2c_t g_i2c;

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

void ha_i2c_init() {

    ha_i2c_t *i2c = &g_i2c;

    i2c->idle_cnt = 0;
    i2c->state = HA_I2C_STATE_ERR;

    // Configure IO pins
    I2C_SDA_PORT &= ~I2C_SDA_MSK;
    I2C_SDA_DIR  &= ~I2C_SDA_MSK;		// Ext Pull-up HIGH

    I2C_SCL_PORT &= ~I2C_SCL_MSK;		// Never change it - SCL either forced-down or ext. pulled-up
    I2C_SCL_DIR  &= ~I2C_SCL_MSK;		// Ext Pull-up HIGH

    // NOTE: From PHTS data sheet:
    // The reset can be sent at any time. In the event that there is
    // not a successful power on reset this may be caused by the SDA
    // being blocked by the module in the acknowledge state. The only
    // way to get the ASIC to function is to send several SCLs followed
    // by a reset sequence or to repeat power on reset.
    // AV: Force several START/STOP conditions according to ^^^^
    scl_delay();
	while(bit_is_clear(I2C_SDA_PIN, I2C_SDA)){

        I2C_SCL_LOW;
        scl_delay();

	    I2C_SCL_HIGH;
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


static void state_on_active_t2()
{
    i2c_trans_t *tr = &g_i2c.trans;

    switch (tr->state) {

        case  I2C_TR_STATE_START:
            break;                                              // T2_START invalid state

        case  I2C_TR_STATE_DATA:                                // T2_DATA -> T3_DATA ... T1_DATA   Set DATA_OUT bit
            if (tr->bit_idx == 0) {
                tr->shift_reg = tr->data[tr->data_idx];
            }

            if (tr->shift_reg & 0x80) {
                I2C_SDA_HIGH;
            } else  {
                I2C_SDA_LOW;
            }
            break;

        case  I2C_TR_STATE_ACK:                                 // T2_ACK -> T3_ACK ... T1_ACK     Set ACK_OUT bit
            if (tr->ack[tr->data_idx]) {
                I2C_SDA_HIGH;
            } else  {
                I2C_SDA_LOW;
            }
            break;

        case  I2C_TR_STATE_STOP:
            I2C_SDA_LOW;                                        // T2_STOP -> T3_STOP     Set DATA LOW
            break;
    }
}

static void state_on_active_t1()
{
    i2c_trans_t *tr = &g_i2c.trans;

    switch (tr->state) {
        case  I2C_TR_STATE_START:                      // T1_START -> T2_DATA
            tr->data_idx = 0;
            tr->bit_idx = 0;

            I2C_SCL_LOW;

            tr->state = I2C_TR_STATE_DATA;
            break;

        case  I2C_TR_STATE_DATA:                        // T1_DATA -> T2_DATA

            // Save data bit
            if (I2C_SDA_PIN & I2C_SDA_MSK) {
                tr->shift_reg |= 1;
            }
            I2C_SCL_LOW;

            if (tr->bit_idx == 8) {
                tr->bit_idx = 0;

                tr->data[tr->data_idx] = tr->shift_reg;
                tr->state = I2C_TR_STATE_ACK;           // T1_DATA ->  T2_ACK
            } else {
                tr->shift_reg <<= 1;
            }
            break;

        case  I2C_TR_STATE_ACK:
            tr->ack[tr->data_idx] = !!(I2C_SDA_PIN & I2C_SDA_MSK);
            // TODO: Interrupt transaction on NACK ?
            // ...

            I2C_SCL_LOW;
            tr->data_idx++;
            tr->bit_idx = 0;
            if (tr->data_idx == tr->len) {
                tr->state = I2C_TR_STATE_STOP;          // T1_ACK ->  T2_STOP
            } else {
                tr->state = I2C_TR_STATE_DATA;          // T1_ACK ->  T2_DATA
            }

            break;

        case  I2C_TR_STATE_STOP:                        // T1_STOP -> finish
            I2C_SDA_HIGH;
            // Transaction finished
            break;
    }
}

static void state_on_active()
{
    i2c_trans_t *tr = &g_i2c.trans;

    switch(tr->timer) {
        case HA_I2C_TIMER_T1:
            state_on_active_t1();                   //
            tr->timer = HA_I2C_TIMER_T2;
            break;

        case HA_I2C_TIMER_T2:
            state_on_active_t2();                   //
            tr->timer = HA_I2C_TIMER_T3;
            break;

        case HA_I2C_TIMER_T3:
            I2C_SCL_HIGH;
            tr->timer = HA_I2C_TIMER_EDGE;
            break;
    }
}

static void state_on_err()
{
#define I2C_IDLE_CNT 10
    ha_i2c_t *i2c = &g_i2c;

    // Check
    if (0 == (I2C_SDA_PIN & I2C_SDA_MSK) ||
        0 == (I2C_SCL_PIN & I2C_SCL_MSK)) {
        i2c->idle_cnt = 0;
    }

    i2c->idle_cnt++;
    if (i2c->idle_cnt == I2C_IDLE_CNT) {
        i2c->state = HA_I2C_STATE_IDLE;
    }
}


void ha_i2c_isr_on_timer()
{
    ha_i2c_t *i2c = &g_i2c;
    i2c_trans_t *tr = &g_i2c.trans;

    switch(i2c->state) {

        case HA_I2C_STATE_ACTIVE:
            state_on_active();
            if ( (tr->state == I2C_TR_STATE_STOP) &&
                 (tr->timer == HA_I2C_TIMER_T2)) {

                // Transaction completed.
                // READY state will be processed in idle loop to avoid long task in ISR
                i2c->state = HA_I2C_STATE_READY;
            }
            break;

        case HA_I2C_STATE_IDLE:
            // Check transaction, initiate transfer if necessary
            if (i2c->trans.data_idx != 0) {
                I2C_SDA_LOW;
                i2c->state = HA_I2C_STATE_ACTIVE;
                i2c->trans.timer = HA_I2C_TIMER_T1;
                i2c->trans.state = I2C_TR_STATE_START;
            }
            break;
        case HA_I2C_STATE_ERR:
            state_on_err();
            break;
    }
}

void ha_i2c_isr_on_scl_edge()
{
    ha_i2c_t *i2c = &g_i2c;
    i2c_trans_t *tr = &g_i2c.trans;

    // Just rewind timer back to T1 on SCL rising edge
    if ( i2c->state == HA_I2C_STATE_ACTIVE &&
         tr->timer == HA_I2C_TIMER_EDGE) {

        tr->timer = HA_I2C_TIMER_T1;
    }
}

void ha_i2c_on_ready()
{
    // call registered call back with data & status code
    // ...
}
