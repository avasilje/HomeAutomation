#pragma once
#include <avr/io.h>
#include <stdint.h>
#include "ha-common.h"

 enum i2c_tr_state_e {
    I2C_TR_STATE_START = 0,
    I2C_TR_STATE_DATA  = 1,
    I2C_TR_STATE_ACK   = 2,
    I2C_TR_STATE_STOP  = 3
 };

#define HA_I2C_STATE_IDLE    0
#define HA_I2C_STATE_ACTIVE  1
#define HA_I2C_STATE_READY   2
#define HA_I2C_STATE_ERR     3

#define HA_I2C_TRANS_LEN   6

#define HA_I2C_TIMER_T1    1
#define HA_I2C_TIMER_T2    2
#define HA_I2C_TIMER_T3    3
#define HA_I2C_TIMER_EDGE  4

 typedef struct i2c_trans_s {
    uint8_t timer;
    enum i2c_tr_state_e state;       // TODO: combine with timer?
    uint8_t len;
    uint8_t shift_reg;
    uint8_t data_idx;
    uint8_t bit_idx;
    uint8_t data[HA_I2C_TRANS_LEN];
    uint8_t cmd[HA_I2C_TRANS_LEN];
    uint8_t ack[HA_I2C_TRANS_LEN];
 } i2c_trans_t;

 typedef struct ha_i2c_s {
    int8_t state;           // I2C_STATE_xxx
    uint8_t idle_cnt;       // Counts number of timer intervals in idle state to restore from error
    i2c_trans_t trans;
 } ha_i2c_t;

void ha_i2c_init();
uint8_t ha_i2c_read_8 (uint8_t addr, uint8_t force_ack, uint8_t *out);
uint8_t ha_i2c_read_16(uint8_t addr, uint8_t force_ack, uint8_t *out);
uint8_t ha_i2c_read_24(uint8_t addr, uint8_t force_ack, uint8_t *out);
uint8_t ha_i2c_write_cmd(uint8_t addr, uint8_t cmd);
uint8_t ha_i2c_write_8(uint8_t addr, uint8_t cmd, uint8_t *data);

void ha_i2c_isr_on_timer();
void ha_i2c_isr_on_scl_edge();
void ha_i2c_on_ready();