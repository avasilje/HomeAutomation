#pragma once
// Att!: IDLE TIMEOUT must be unique for a particular device
#define NLINK_IO_IDLE_TIMEOUT_CEIL      10
#define NLINK_IO_IDLE_TIMEOUT_AMBILED   12
#define NLINK_IO_IDLE_TIMEOUT_BLCN      14
// #define NLINK_IO_IDLE_TIMEOUT_BATHROOM  

/* Shared with AMBILED/CEIL */
#define SWITCH_ADDR_CEIL    (SWITCH_ADDR+0)
#define SWITCH_ADDR_AMBILED (SWITCH_ADDR+1)
#define SWITCH_ADDR_BLCN    (SWITCH_ADDR+2)

#define LEDLIGHT_ADDR_CEIL  (LEDLIGHT_ADDR+0)
#define LEDLIGHT_ADDR_ALED  (LEDLIGHT_ADDR+5)
#define LEDLIGHT_ADDR_BLCN  (LEDLIGHT_ADDR+6)

#define ROLLS_ADDR_BLCN     (ROLL_ADDR+0)

// Device specific part. Proper device selected by preprocessor defines
#include "ha-common-father-blcn.h"
#include "ha-common-father-ceil.h"
#include "ha-common-father-bathroom.h"
#include "ha-common-father-ambiled.h"
