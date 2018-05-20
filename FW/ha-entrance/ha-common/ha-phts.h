#pragma once

// MS860702BA01 - PHT Combination Sensor

// For pressure and temperature sensing, five commands are possible:





#define HA_PHTS_CMD_PT_RESET   0x1E     // 1. Reset
#define HA_PHTS_CMD_PT_RDCAL   0xA0     // 2. Read PROM P&T (112 bit of calibration words)
#define HA_PHTS_CMD_PT_D1CON   0x40     // 3. D1 conversion
#define HA_PHTS_CMD_PT_D2CON   0x50     // 4. D2 conversion
#define HA_PHTS_CMD_PT_RDADC   0x00     // 5. Read ADC (24 bit pressure / temperature)

// Resolution add-on for D1/D2 conversion command
#define HA_PHTS_CMD_PT_OSR_256     0x00
#define HA_PHTS_CMD_PT_OSR_512     0x02
#define HA_PHTS_CMD_PT_OSR_1024    0x04
#define HA_PHTS_CMD_PT_OSR_2048    0x06
#define HA_PHTS_CMD_PT_OSR_4096    0x08
#define HA_PHTS_CMD_PT_OSR_8192    0x0A

// For relative humidity sensing, six commands are possible:
#define HA_PHTS_CMD_RH_RESET    0xFE    // 1. Reset
#define HA_PHTS_CMD_RH_WRUSR    0xE6    // 2. Write user register
#define HA_PHTS_CMD_RH_RDUSR    0xE7    // 3. Read user register
#define HA_PHTS_CMD_RH_MSRHM    0xE5    // 4. Measure RH (Hold master)
#define HA_PHTS_CMD_RH_MSRNH    0xF5    // 5. Measure RH (No Hold master)
#define HA_PHTS_CMD_RH_RDCAL    0xA0    // 6. PROM read RH

typedef struct phts_s {
   int8_t poll_timer;
   // Calibration data
} phts_t;

void ha_phts_poll(phts_t *sensor);
void ha_phts_init();

