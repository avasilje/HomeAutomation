/*
 * ha_common_xxx.h
 *
 * Created: 5/12/2018 6:57:31 PM
 *  Author: solit
 */
#ifdef HA_DEV_LOGGIA

#ifndef HA_COMMON_XXX_H_
#define HA_COMMON_XXX_H_

#include <avr/interrupt.h>
#include <avr/io.h>

#define NLINK_NODES_NUM 1       // HVAC + TBD(SWITCH)

#define NLINK_IO_IDLE_TIMEOUT NLINK_IO_IDLE_TIMEOUT_LOGGIA // timeout when TX can start to transmit.

// ATTiny4313
#define ENABLE_PCINT  do {GIFR |= (1 << PCIF0); GIMSK |= (1 << PCIE0); } while(0)
#define DISABLE_PCINT  GIMSK &= ~(1 << PCIE0)

#define NLINK_RX_INT_ENABLE ENABLE_PCINT
#define NLINK_RX_INT_DISABLE DISABLE_PCINT

extern int8_t g_ha_nlink_timer_cnt;
#define NLINK_IO_TIMER_ENABLE  g_ha_nlink_timer_cnt = 0
#define NLINK_IO_TIMER_DISABLE g_ha_nlink_timer_cnt = -1

// ATTiny4313
#define NLINK_IO_RX_PORT  PORTB
#define NLINK_IO_RX_DIR   DDRB
#define NLINK_IO_RX_PIN   PINB
#define NLINK_IO_RX_PIN_MASK   _BV(PINB0)   // PCINT0

#define NLINK_IO_TX_PORT  PORTB
#define NLINK_IO_TX_DIR   DDRB
#define NLINK_IO_TX_PIN_MASK   _BV(PINB1)

#define NLINK_IO_DBG_PIN0_MASK _BV(PIND0)
#define NLINK_IO_DBG_PIN1_MASK _BV(PIND1)
#define NLINK_IO_DBG_PORT PORTD
#define NLINK_IO_DBG_PIN  PIND
#define NLINK_IO_DBG_DIR  DDRD

#endif /* HA_COMMON_XXX_H_ */
#endif /* HA_DEV_LOGGIA */