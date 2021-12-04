#pragma once
// Att!: IDLE TIMEOUT must be unique for a particular device
#define NLINK_IO_IDLE_TIMEOUT_ELBOX     10
#define NLINK_IO_IDLE_TIMEOUT_ENTRANCE  12
#define NLINK_IO_IDLE_TIMEOUT_LOGGIA    14

// Device specific part. Proper device selected by preprocessor defines
#include "ha-common-lounge-elbox.h"
#include "ha-common-lounge-entrance.h"
#include "ha-common-lounge-loggia.h"
