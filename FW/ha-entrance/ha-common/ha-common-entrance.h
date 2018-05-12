/*
 * ha_common_xxx.h
 *
 * Created: 5/12/2018 6:57:31 PM
 *  Author: solit
 */
#ifdef HA_DEV_ENTRANCE

#ifndef HA_COMMON_XXX_H_
#define HA_COMMON_XXX_H_

#include <avr/interrupt.h>
#include <avr/io.h>

#define NLINK_NODES_NUM 3       // CTRLCON + SWITCH + ???

#define NLINK_IO_IDLE_TIMEOUT 12    // timeout when TX can start to transmit.
// TODO: ^^^ Must be unique for a particular device

// AVR32
#define NLINK_RX_INT_ENABLE do { GIFR |= INTF0; GICR |= _BV(INT0);} while(0)
#define NLINK_RX_INT_DISABLE GICR &= ~_BV(INT0)

#define NLINK_IO_TIMER_ENABLE  do {TIFR |= _BV(TOV2); TIMSK |= _BV(TOIE2);} while(0)
#define NLINK_IO_TIMER_DISABLE TIMSK &= ~_BV(TOIE2)

#define NLINK_IO_RX_PORT  PORTD
#define NLINK_IO_RX_DIR   DDRD
#define NLINK_IO_RX_PIN   PIND
#define NLINK_IO_RX_PIN_MASK   _BV(PIND2)   // INT0

#define NLINK_IO_TX_PORT  PORTB
#define NLINK_IO_TX_DIR   DDRB
#define NLINK_IO_TX_PIN_MASK   _BV(PINB1)

extern int8_t g_ha_nlink_timer_cnt;

#define NLINK_IO_DBG_PIN_MASK _BV(PIND0)
#define NLINK_IO_DBG_PORT PORTD
#define NLINK_IO_DBG_PIN  PIND
#define NLINK_IO_DBG_DIR  DDRD


#endif /* HA_COMMON_XXX_H_ */
#endif /* HA_DEV_ELBOX */