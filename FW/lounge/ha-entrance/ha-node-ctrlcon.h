/*
 * ha_node_ctrlcon.h
 *
 * Created: 5/1/2018 11:31:42 PM
 *  Author: Solit
 */
#pragma once

extern void ha_node_ctrlcon_init();
extern uint8_t ha_node_ctrlcon_on_tx_uart(uint8_t *buf);		// Returns node info to be send to UART (ctrlcon_on_tx_uart???) Q:?Register it in ha_uart?
extern void ha_node_ctrlcon_on_rx_uart(const uint8_t *data_in, uint8_t len);
extern void ha_node_ctrlcon_discover();

extern void ha_node_ctrlcon_peer_set_rx(uint8_t remote_peer_addr);		// mark node as just updated from NLINK
