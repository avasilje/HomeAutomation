#pragma once
/*
 * ha_uart.h
 *
 * Created: 6/25/2018 1:02:48 PM
 *  Author: andrejs.vasiljevs
 */ 

#define HA_UART_RX_BUFF_SIZE 32
#define HA_UART_TX_BUFF_SIZE 32

#define HA_UART_SYNC_TIMEOUT 20		// in 10ms intervals
// NLINK_COMM_BUF_SIZE

typedef struct ha_uart_s {

	int8_t syncing_timer;			// -1 - normal operation; 0 - resync in progress; >0 resync delay
	uint8_t resync_cnt;				// Just for debugging purposes
	
	uint8_t rx_wr_idx;
	uint8_t rx_buff[HA_UART_RX_BUFF_SIZE];

	// 0x41 0x86 0x22  0xLL    |
	// MARK      CMD   Length  |

	uint8_t tx_buf[HA_UART_TX_BUFF_SIZE];
	uint8_t tx_rd_idx;
	uint8_t tx_len;
} ha_uart_t;

extern ha_uart_t ha_uart;


#define UART_CC_HDR_MARK0 0
#define UART_CC_HDR_MARK1 1
#define UART_CC_HDR_CMD   2
#define UART_CC_HDR_LEN   3
#define UART_CC_HDR_DATA  4
#define UART_CC_HDR_SIZE  (UART_CC_HDR_DATA)

extern void ha_uart_init();
extern void ha_uart_enable_tx();	// ???  is in use
extern void ha_uart_resync();
extern void ha_uart_check_tx();
extern void ha_uart_check_rx();
extern void ha_uart_on_timer();

// temp fix 
// TODO: register or get as a callback on init?
extern void command_lookup(uint8_t uc_msg_cmd);				// ha_entrance_on_rx_uart
extern uint8_t ha_node_ctrlcon_on_tx_uart(uint8_t *buf);	// ha_entrance_on_tx_uart
