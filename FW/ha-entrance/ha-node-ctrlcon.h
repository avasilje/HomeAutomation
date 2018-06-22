/*
 * ha_node_ctrlcon.h
 *
 * Created: 5/1/2018 11:31:42 PM
 *  Author: Solit
 */ 
#pragma once

extern void ha_node_ctrlcon_init();
extern uint8_t ha_node_ctrlcon_to_sent(uint8_t *buf);
extern void ha_node_ctrlcon_peer_set_rx(uint8_t remote_peer_addr);