#include <avr/io.h>
#include <stdlib.h>
#include <avr/interrupt.h>

#include "ha-common.h"
#include "ha-uart.h"

ha_uart_t ha_uart;

void ha_uart_init()
{
	// Init UART to 250000Mbps 8N1

	// Set baud rate. UBRR = Fosc/8/BAUD - 1
	// Fosc = 8MHz, UBRR = 8*10^6/16/250000 - 1 = 1
	UBRRH = 0;
	UBRRL = 1;

	// Set frame format: 8data, 1stop bit, no parity */
	UCSRC =
	_BV(URSEL) |    // Enable access to UCSRC. Otherwise UBRRH will be overwritten
	_BV(UCSZ0) |    // 8 data bits
	_BV(UCSZ1);     // 8 data bits

	// Enable receiver and transmitter
	UCSRB =
	_BV(RXCIE)|     // RX Interrupt enable
	_BV(RXEN) |     // Receiver enabled
	_BV(TXEN);      // Transmitter enabled

	ha_uart.resync_cnt = 0;
	ha_uart_resync();
}

/* TODO: Q: is in use? */
void ha_uart_enable_tx()
{
	UCSRB |= _BV(UDRIE);  // TX Interrupt enable
}

void ha_uart_resync()
{
	// Flush RX buffer
	volatile uint8_t dev_null;
	while ( UCSRA & (1<<RXC) ) dev_null = UDR;
	UNREFERENCED_PARAM(dev_null);

	ha_uart.rx_wr_idx = 0;
	ha_uart.tx_rd_idx = 0;
	ha_uart.tx_len = 0;
	ha_uart.syncing_timer = HA_UART_SYNC_TIMEOUT;
	ha_uart.resync_cnt++;
}

void ha_uart_check_tx()
{
	// Check CTRLCON TX if UART TX is disabled
	if (0 == (UCSRB & _BV(UDRIE))) {
		// Nothing to sent - check ctrlcon

		if (ha_uart.syncing_timer < 0) {
			uint8_t bytes_to_send = ha_node_ctrlcon_on_tx_uart(&ha_uart.tx_buf[UART_CC_HDR_SIZE]);
			if (bytes_to_send) {
				ha_uart.tx_buf[UART_CC_HDR_MARK0] = 0x41;
				ha_uart.tx_buf[UART_CC_HDR_MARK1] = 0x86;
				ha_uart.tx_buf[UART_CC_HDR_CMD  ] = 0x21;
				ha_uart.tx_buf[UART_CC_HDR_LEN  ] = bytes_to_send;
				ha_uart.tx_len = bytes_to_send + UART_CC_HDR_SIZE;

				ha_uart.tx_rd_idx = 0;
				UCSRB |= _BV(UDRIE);
            }
		} else {
			// Send "need sync command" if timeout expired
			if (ha_uart.syncing_timer == 0) {
				// Send sync command and start collecting data
				ha_uart.syncing_timer = -1;
				ha_uart.tx_buf[UART_CC_HDR_MARK0] = 0x41;
				ha_uart.tx_buf[UART_CC_HDR_MARK1] = 0x86;
				ha_uart.tx_buf[UART_CC_HDR_CMD  ] = 0x31;
				ha_uart.tx_buf[UART_CC_HDR_LEN  ] = 0;
				ha_uart.tx_len = UART_CC_HDR_SIZE;
				ha_uart.tx_rd_idx = 0;
				UCSRB |= _BV(UDRIE);
			}
		}
	}
}

void ha_uart_check_rx()
{
	// Get number of bytes received (4 bytes at least)
	if (ha_uart.syncing_timer <= 0 && ha_uart.rx_wr_idx >= 4) {

		// check is command valid
		// MARK = "AV", read pointer must be set on buffer start
		if (( ha_uart.rx_buff[UART_CC_HDR_MARK0] != 0x41) ||
			( ha_uart.rx_buff[UART_CC_HDR_MARK1] != 0x86)) {
			ha_uart_resync();
            return;
		}

		uint8_t msg_cmd = ha_uart.rx_buff[UART_CC_HDR_CMD];

		command_lookup(msg_cmd);

		// Clear RX buffer index in order to receive next message
		ha_uart.rx_wr_idx = 0;
	}
}

void ha_uart_on_timer()
{
	if (ha_uart.syncing_timer > 0) {
		ha_uart.syncing_timer--;
	}
}

ISR(USART_RXC_vect) {

	uint8_t uc_status;
	uint8_t uc_data;

	// get RX status, trap if something wrong
	uc_status = UCSRA;
	uc_data = UDR;

	// Check Frame Error (FE0) and DATA overflow (DOR0)
	if ( uc_status & (_BV(FE) | _BV(DOR)) ) {
		ha_uart_resync();
    }

	if ( ha_uart.rx_wr_idx == HA_UART_RX_BUFF_SIZE ) {
		ha_uart_resync();
    }

	if (ha_uart.syncing_timer <= 0) {
		// Save data only if in sync or syncing
		ha_uart.rx_buff[ha_uart.rx_wr_idx++] = uc_data;
	}

}

ISR(USART_UDRE_vect) {

	// Unload TX buffer if not empty
	if (ha_uart.tx_rd_idx != ha_uart.tx_len) {
		UDR = ha_uart.tx_buf[ha_uart.tx_rd_idx];
		UCSRA |= (1 << TXC); // Clear transmit complete flag
		ha_uart.tx_rd_idx ++;
		if (ha_uart.tx_rd_idx == ha_uart.tx_len) {
			// No more data - disable interrupt
			UCSRB &= ~_BV(UDRIE);
		}
		return;
	}
}
