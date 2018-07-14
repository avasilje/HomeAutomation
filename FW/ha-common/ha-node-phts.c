/*
 * ha_node_phts.c
 *
 */
#include <avr/io.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "ha-common.h"
#include "ha-nlink.h"
#include "ha-i2c.h"
#include "ha-phts.h"
#include "ha-node-phts.h"

node_t *g_phts_nlink_node = (void*)0;
uint8_t g_phts_poll_flag = 0;
uint8_t g_phts_cmd = 0;

ha_phts_t   g_ha_phts_sensor;
uint8_t prev_state;
uint8_t prev_read_cnt;
uint8_t prev_prom_rd_idx;

void phts_on_rx(uint8_t idx, const uint8_t *buf_in)
{
    UNREFERENCED_PARAM(idx);

    // Reply with full answer - calibration coef + adc values
    node_t *node = g_phts_nlink_node;
    ha_phts_t *phts = &g_ha_phts_sensor;
    uint8_t rc;
    uint8_t rd_idx;

    if (buf_in[NLINK_HDR_OFF_TYPE] != NODE_TYPE_PHTS) {
        // Unexpected event type
        return;
    }

    switch (buf_in[NLINK_HDR_OFF_DATA + PHTS_CMD_OFF]) {
        case PHTS_CMD_RESET_PT:
            rc = ha_phts_reset_pt(phts);
            break;
        case PHTS_CMD_RESET_RH:
            rc = ha_phts_reset_rh(phts);
            break;
        case PHTS_CMD_PROM_RD:
			rd_idx = buf_in[NLINK_HDR_OFF_DATA + PHTS_CMD_PROM_RD_IDX_OFF];
            rc = ha_phts_prom_rd(phts, rd_idx);
			break;
        case PHTS_CMD_MEASURE:
            rc = ha_phts_measurement_start(phts);
			break;
    }

    if (rc) {
        // Report an error
        // ...
    }
}

void ha_node_phts_init()
{
    // Set INT1 trigger to Rising Edge, Clear interrupt flag
    //MCUCR &= ~(_BV(ISC10) | _BV(ISC11));
    //MCUCR |= (3 << ISC10);

    I2C_SCL_INT_PORT &= ~I2C_SCL_INT_MSK;
    I2C_SCL_INT_DIR  &= ~I2C_SCL_INT_MSK;   // input no pull-up (external pull-up assumed)

    ha_phts_init(&g_ha_phts_sensor);
    g_ha_phts_sensor.d1_poll_timeout = 2;
    g_ha_phts_sensor.d2_poll_timeout = 2;
    g_ha_phts_sensor.idle_timeout = 100;

    g_phts_nlink_node = ha_nlink_node_register(PHTS_ADDR, NODE_TYPE_PHTS, phts_on_rx, (node_tx_cb_t)NULL);

    // Clear TX buffer
    node_t *node = g_phts_nlink_node;
    node->tx_buf[NLINK_HDR_OFF_LEN] = 0;
    node->tx_buf[NLINK_HDR_OFF_TYPE] = NODE_TYPE_PHTS;
}

void ha_node_phts_on_timer()
{
    g_phts_poll_flag = 1;
}

void ha_node_phts_on_idle() {

    node_t *node = g_phts_nlink_node;
    ha_phts_t *phts = &g_ha_phts_sensor;
    uint8_t len = 0;

	if (g_phts_poll_flag == 0) {
        return;
    }
	g_phts_poll_flag = 0;

    // Execute phts sensor polling ~@10ms
    ha_phts_measurement_poll(&g_ha_phts_sensor);

    if ( (phts->state == PHTS_STATE_IDLE) &&
         (phts->readings.read_cnt != prev_read_cnt) ) {

         prev_read_cnt = phts->readings.read_cnt;

        // New measurement available
        memcpy(&node->tx_buf[NLINK_HDR_OFF_DATA + PHTS_READINGS], (uint8_t*)&phts->readings, sizeof(ha_phts_readings_t));

        len = 1 + sizeof(ha_phts_readings_t);

    } else if (phts->state < PHTS_STATE_PROM_RD && phts->state != prev_state) {
        // RESET completed or init
        len = 1;

    } else if ( (phts->state == PHTS_STATE_PROM_RD) &&
                (phts->prom_rd_idx != prev_prom_rd_idx) ) {

        prev_prom_rd_idx = phts->prom_rd_idx;

        // New PROM coefficient read
        uint8_t addr = phts->prom_rd_idx;
        node->tx_buf[NLINK_HDR_OFF_DATA + PHTS_PROM_RD_IDX] = addr;
        node->tx_buf[NLINK_HDR_OFF_DATA + PHTS_PROM_COEFF + 0] = phts->pt_prom.raw8[addr + 0];
        node->tx_buf[NLINK_HDR_OFF_DATA + PHTS_PROM_COEFF + 1] = phts->pt_prom.raw8[addr + 1];
        len = 4;
    }

    if (len) {
        node->tx_buf[NLINK_HDR_OFF_DATA + PHTS_STATE] = phts->state;
        node->tx_buf[NLINK_HDR_OFF_LEN] = len;
        ha_nlink_node_send(node, LEDLIGHT_ADDR, NLINK_CMD_INFO);
    }
    prev_state = phts->state;

}
