#pragma once
#include <avr/io.h>
#include <stdint.h>
#include "ha-common.h"

 enum i2c_tr_state_e {
    I2C_STATE_IDLE   = 0,    // Ready to start
    I2C_STATE_ACTIVE = 1,    // In progress - interrupt enabled
    I2C_STATE_READY  = 2,    // Result is available
    I2C_STATE_ERROR  = 3     // Error
 };

 typedef struct i2c_s {
    int8_t state;

 } i2c_t;

void ha_i2c_init();
uint8_t ha_i2c_read_8 (uint8_t addr, uint8_t force_ack, uint8_t *out);
uint8_t ha_i2c_read_16(uint8_t addr, uint8_t force_ack, uint8_t *out);
uint8_t ha_i2c_read_24(uint8_t addr, uint8_t force_ack, uint8_t *out);
uint8_t ha_i2c_write_cmd(uint8_t addr, uint8_t cmd);
uint8_t ha_i2c_write_8(uint8_t addr, uint8_t cmd, uint8_t *data);

