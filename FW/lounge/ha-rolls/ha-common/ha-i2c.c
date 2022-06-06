/*
 * TODO:
 *   1. Add REPEATED START condition
 *   2. Add transaction interrupt on NACK
 *   3. Add call back processing on result ready
 */
#include <stddef.h>
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

static void state_on_active_t2()
{
    i2c_trans_t *tr = &g_i2c.trans;

    switch (tr->state) {

        case I2C_TR_STATE_START:
            I2C_SDA_LOW;
            tr->timer = HA_I2C_TIMER_T1;
            break;                                              // T2_START

        case I2C_TR_STATE_DATA:                                // T2_DATA -> T3_DATA ... T1_DATA   Set DATA_OUT bit
            if (tr->bit_idx == 0) {
                tr->shift_reg = tr->data[tr->data_idx];
            }

            if (tr->shift_reg & 0x80) {
                I2C_SDA_HIGH;
            } else  {
                I2C_SDA_LOW;
            }
            tr->shift_reg <<= 1;
            tr->timer = HA_I2C_TIMER_T3;
            break;

        case I2C_TR_STATE_ACK:                                 // T2_ACK -> T3_ACK ... T1_ACK     Set ACK_OUT bit
            if (tr->ack[tr->data_idx]) {
                I2C_SDA_HIGH;
            } else  {
                I2C_SDA_LOW;
            }
            tr->timer = HA_I2C_TIMER_T3;
            break;

        case I2C_TR_STATE_STOP:
            I2C_SDA_LOW;                                        // T2_STOP -> T3_STOP     Set DATA LOW
            tr->timer = HA_I2C_TIMER_T3;
            break;
        default:
            break;

    }
}

static void state_on_active_t1()
{
    i2c_trans_t *tr = &g_i2c.trans;

    switch (tr->state) {
        case I2C_TR_STATE_START:                      // T1_START -> T2_DATA
            tr->bit_idx = 0;

            I2C_SCL_LOW;

            tr->state = I2C_TR_STATE_DATA;
            break;

        case I2C_TR_STATE_DATA:                        // T1_DATA -> T2_DATA

            // Save data bit
            if (I2C_SDA_PIN & I2C_SDA_MSK) {
                tr->shift_reg |= 1;
            }
            I2C_SCL_LOW;
            tr->bit_idx ++;
            if (tr->bit_idx == 8) {
                tr->bit_idx = 0;

                tr->data[tr->data_idx] = tr->shift_reg;
                tr->state = I2C_TR_STATE_ACK;           // T1_DATA ->  T2_ACK
            }

            break;

        case I2C_TR_STATE_ACK:
            tr->ack[tr->data_idx] = !!(I2C_SDA_PIN & I2C_SDA_MSK);
            // TODO: Interrupt transaction on NACK ?
            // ...

            I2C_SCL_LOW;
            tr->bit_idx = 0;
			// TODO: rework to REPEATED STOP/START transaction
            if (tr->stop_bit[tr->data_idx] == 1) {
                tr->state = I2C_TR_STATE_STOP;          // T1_ACK ->  T2_STOP
            } else {
                tr->state = I2C_TR_STATE_DATA;          // T1_ACK ->  T2_DATA
            }
            tr->data_idx++;
            if (tr->data_idx > tr->len)  {
                FATAL_TRAP(__LINE__);
            }
            break;

        case I2C_TR_STATE_STOP:                        // T1_STOP -> finish
            I2C_SDA_HIGH;
            if (tr->data_idx == tr->len) {
                tr->state = I2C_TR_STATE_FINISH;
			} else {
                tr->state = I2C_TR_STATE_START;         // T1_STOP -> T2_START
			}
            break;
        default:
            break;
    }
}

static void state_on_active()
{
    i2c_trans_t *tr = &g_i2c.trans;

    switch(tr->timer) {
        case HA_I2C_TIMER_T1:
            state_on_active_t1();                   //  SCL falling edge
            tr->timer = HA_I2C_TIMER_T2;
            break;

        case HA_I2C_TIMER_T2:
            state_on_active_t2();                   // SCL low mid-point
            break;

        case HA_I2C_TIMER_T3:                       // SCL Rising edge
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
            if ( tr->state == I2C_TR_STATE_FINISH &&
                 tr->timer == HA_I2C_TIMER_T2) {

                // Transaction completed.
                // READY state will be processed in idle loop to avoid long task in ISR
                i2c->state = HA_I2C_STATE_READY;
            }
            break;

        case HA_I2C_STATE_IDLE:
            // Check transaction, initiate transfer if necessary
            if (tr->len != 0) {
                i2c->state = HA_I2C_STATE_ACTIVE;
                tr->timer = HA_I2C_TIMER_T2;
                tr->state = I2C_TR_STATE_START;
                tr->data_idx = 0;
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


int8_t ha_i2c_cmd(uint8_t addr, uint8_t cmd, i2c_tr_completed_cb_t completed_cb, void* cb_ctx, uint8_t cb_param)
{
    ha_i2c_t *i2c = &g_i2c;
    i2c_trans_t *tr = &g_i2c.trans;

	if (i2c->state != HA_I2C_STATE_IDLE) {
		return -1;
	}

	tr->data[0] = addr & (~0x01);
	tr->data[1] = cmd;
	tr->ack[0] = 1;
	tr->ack[1] = 1;
    tr->stop_bit[0] = 0;
    tr->stop_bit[1] = 1;
	tr->cb = completed_cb;
	tr->cb_ctx = cb_ctx;
	tr->cb_param = cb_param;
	tr->len = 2;    // Initiates transaction
    return 0;
}

int8_t ha_i2c_read16(uint8_t addr, uint8_t cmd, i2c_tr_completed_cb_t completed_cb, void* cb_ctx, uint8_t cb_param)
{
	ha_i2c_t *i2c = &g_i2c;
	i2c_trans_t *tr = &g_i2c.trans;

	if (i2c->state != HA_I2C_STATE_IDLE) {
		return -1;
	}

	tr->data[0] = addr;
	tr->data[1] = cmd;
	tr->data[2] = addr | 0x01;
	tr->data[3] = 0xFF;
	tr->data[4] = 0xFF;
    tr->stop_bit[0] = 0;
    tr->stop_bit[1] = 1;
    tr->stop_bit[2] = 0;
    tr->stop_bit[3] = 0;
    tr->stop_bit[4] = 1;

	tr->ack[0] = 1;
	tr->ack[1] = 1;
	tr->ack[2] = 1;
	tr->ack[3] = 0;
	tr->ack[4] = 1;

	tr->cb = completed_cb;
	tr->cb_ctx = cb_ctx;
	tr->cb_param = cb_param;
    tr->len = 5;    // Initiates transaction

    return 0;
}

int8_t ha_i2c_read24(uint8_t addr, uint8_t cmd, i2c_tr_completed_cb_t completed_cb, void* cb_ctx, uint8_t cb_param)
{
	ha_i2c_t *i2c = &g_i2c;
	i2c_trans_t *tr = &g_i2c.trans;

	if (i2c->state != HA_I2C_STATE_IDLE) {
		return -1;
	}

	tr->data[0] = addr;				// WR
	tr->data[1] = cmd;
	tr->data[2] = addr | 0x01;		// RD
	tr->data[3] = 0xFF;
	tr->data[4] = 0xFF;
	tr->data[5] = 0xFF;
    tr->stop_bit[0] = 0;
    tr->stop_bit[1] = 1;
    tr->stop_bit[2] = 0;
    tr->stop_bit[3] = 0;
    tr->stop_bit[4] = 0;
    tr->stop_bit[5] = 1;

	tr->ack[0] = 1;
	tr->ack[1] = 1;
	tr->ack[2] = 1;
	tr->ack[3] = 0;
	tr->ack[4] = 0;
	tr->ack[5] = 1;

	tr->cb = completed_cb;
	tr->cb_ctx = cb_ctx;
	tr->cb_param = cb_param;
    tr->len = 6;    // Initiates transaction
    return 0;
}

void ha_i2c_on_idle()
{
    ha_i2c_t *i2c = &g_i2c;
    i2c_trans_t *tr = &g_i2c.trans;

    if (i2c->state == HA_I2C_STATE_READY) {
        tr->len = 0;
        i2c->state = HA_I2C_STATE_IDLE;
        tr->cb(tr);
    }

}