#include "ha-phts.h"

void ha_phts_poll(phts_t *sensor) {

#if 0
    if (sensor->poll_timer > 0 ) {
        sensor->poll_timer --;
        return;
    }

    if (g_i2c.state == I2C_STATE_IDLE) {
        // Initiate new transaction
        ha_i2c_read(HVAC_PHTS_I2C_ADDR_PT, HA_PHTS_CMD_);
    }
    // It is time to get sensor data
#endif
}

void ha_phts_init() {

    // Send Reset command to the sensor

    // Read sensor's calibration coefficients

}
