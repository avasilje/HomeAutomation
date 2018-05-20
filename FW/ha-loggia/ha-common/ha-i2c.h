#pragma once


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
