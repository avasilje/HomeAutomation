/*
 * ha_ndoe_ctrlcon.c
 *
 * Created: 5/1/2018 11:26:18 PM
 *  Author: Solit
 */ 
#include <avr/interrupt.h>
#include <stdint.h>
#include <string.h>
#include "ha-common.h"
#include "ha-entrance.h"
#include "ha-nlink.h"

// HVAC DATA
//      TYPE(HVAC) HEATER(%), VALVE(ON/OFF), MOTOR (I/II/OFF), TEMP, PRES, HUM.
//  
typedef struct ctrlcon_peer_s {
    uint8_t idx;            // Discovered node's index. Filled at register. Used while unload to UART toward UI
    uint8_t peer_flags;     // RX flag NODE_FLAG_xxx
    uint8_t remote_addr;
    uint8_t type;
    uint8_t rx_buf[NLINK_COMM_BUF_SIZE];
    uint8_t tx_buf[NLINK_COMM_BUF_SIZE];
} ctrlcon_peer_t;

#define PEER_FLAG_VALID 1      // for sanity purposes
#define PEER_FLAG_RX    2      // Set on update from nlink, cleared upon processed by ctrlcon UART

#define CTRLCON_PEERS_NUM 3    // HVAC + SWITCH + LEDLIGHT
typedef struct ctrlcon_s {
    node_t *cc_node;
    ctrlcon_peer_t peers[CTRLCON_PEERS_NUM];
} ctrlcon_t;

ctrlcon_t cc;

static void ctrlcon_new_peer(uint8_t peer_idx, uint8_t remote_addr, uint8_t type) {
    cc.peers[peer_idx].idx = peer_idx; 
    cc.peers[peer_idx].peer_flags = PEER_FLAG_VALID;
    cc.peers[peer_idx].remote_addr = remote_addr;
    cc.peers[peer_idx].type = type;
}

void ctrlcon_on_rx(uint8_t idx, const uint8_t *buf_in) 
{
    UNREFERENCED_PARAM(idx);
    uint8_t peer_idx;
    uint8_t remote_peer_addr = buf_in[NLINK_HDR_OFF_FROM];

    // Check is peer already exists
    ctrlcon_peer_t *peer = &cc.peers[0];
    for (peer_idx = 0; peer_idx < ARRAY_SIZE(cc.peers); peer_idx++, peer++) {
	    if (peer->remote_addr == 0) {
		    // Peer not found - create new
            ctrlcon_new_peer(peer_idx, remote_peer_addr, buf_in[NLINK_HDR_OFF_TYPE]);
  		    break;
	    }
	    if (peer->remote_addr == remote_peer_addr) {
		    break;
	    }
    }

    if (peer_idx == ARRAY_SIZE(cc.peers)) {
	    FATAL_TRAP(__LINE__); // Peer not found and new can't be created
    }

    memcpy(peer->rx_buf, buf_in, NLINK_COMM_BUF_SIZE);
    peer->peer_flags |= PEER_FLAG_RX;
}

uint8_t ha_node_ctrlcon_to_sent(uint8_t *buf) 
{
    // Prepare data to be sent to UART
    for (uint8_t peer_idx = 0; peer_idx < ARRAY_SIZE(cc.peers); peer_idx++) {
        ctrlcon_peer_t *peer = &cc.peers[peer_idx];
        cli();
            if (peer->peer_flags & PEER_FLAG_RX) {
                peer->peer_flags &= ~PEER_FLAG_RX;
                uint8_t len = peer->rx_buf[NLINK_HDR_OFF_LEN] + NLINK_HDR_OFF_DATA;
                memcpy(buf, peer->rx_buf, len);
                sei();
                return len;
            }
        sei();
    }
    return 0;
}

void ha_node_ctrlcon_init() 
{
    memset((uint8_t*)&cc, 0, sizeof(ctrlcon_t));

    cc.cc_node = ha_nlink_node_register(CTRLCON_ADDR, NODE_TYPE_CTRLCON, ctrlcon_on_rx);
    
    // Send initial discovery.
    // All remote peers should respond with CTRLCON_ADDR in destination
    ha_nlink_node_send(cc.cc_node, NODE_ADDR_BC, NLINK_CMD_RD_REQ);
}
