#pragma once
// Att!: IDLE TIMEOUT must be unique for a particular device
#define NLINK_IO_IDLE_TIMEOUT_ELBOX     10
#define NLINK_IO_IDLE_TIMEOUT_ENTRANCE  12
#define NLINK_IO_IDLE_TIMEOUT_LOGGIA    14  // Obsolete????
#define NLINK_IO_IDLE_TIMEOUT_LBLCN     16

#define SWITCH_ADDR_BLCN    (SWITCH_ADDR+0)
#define LEDLIGHT_ADDR_BLCN  (LEDLIGHT_ADDR+0)
#define ROLL_D_ADDR_BLCN    (ROLL_ADDR+0)
#define ROLL_W_ADDR_BLCN    (ROLL_ADDR+1)

// Device specific part. Proper device selected by preprocessor defines
#include "ha-common-lounge-elbox.h"
#include "ha-common-lounge-entrance.h"
#include "ha-common-lounge-loggia.h"
#include "ha-common-lounge-blcn.h"
