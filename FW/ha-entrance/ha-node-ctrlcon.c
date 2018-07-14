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
#include "ha-uart.h"

// HVAC DATA
//      TYPE(HVAC) HEATER(%), VALVE(ON/OFF), MOTOR (I/II/OFF), TEMP, PRES, HUM.
//
typedef struct ctrlcon_peer_s {
    uint8_t idx;            // Discovered node's index. Filled at register. Used while unload to UART toward UI
    uint8_t peer_flags;     // RX flag NODE_FLAG_xxx
    uint8_t remote_addr;
    uint8_t type;
    uint8_t peer_rx_buf[NLINK_COMM_BUF_SIZE];
    uint8_t peer_tx_buf[NLINK_COMM_BUF_SIZE];
} ctrlcon_peer_t;

#define PEER_FLAG_VALID		1   // for sanity purposes
#define PEER_FLAG_RX_NLINK	2	// Set on update from nlink, cleared upon processed by ctrlcon UART
#define PEER_FLAG_RX_UART	4	// Set on update from uart, cleared upon processed by ctrlcon nlink

#define CC_TX_FLAG_PEERS    1
#define CC_TX_FLAG_NODE     2

#define CTRLCON_PEERS_NUM 4    // HVAC + SWITCH + LEDLIGHT + PHTS
typedef struct ctrlcon_s {
    node_t *cc_node;
    ctrlcon_peer_t peers[CTRLCON_PEERS_NUM];
    uint8_t cc_tx_flag;        // CTRLCON overrides nlink TX function - so it uses it's own tx_flag
} ctrlcon_t;

ctrlcon_t cc;

static void ctrlcon_new_peer(uint8_t peer_idx, uint8_t remote_addr, uint8_t type) {
    cc.peers[peer_idx].idx = peer_idx;
    cc.peers[peer_idx].peer_flags = PEER_FLAG_VALID;
    cc.peers[peer_idx].remote_addr = remote_addr;
    cc.peers[peer_idx].type = type;
}

void ctrlcon_on_rx_nlink(uint8_t idx, const uint8_t *buf_in)
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

/*
    // Proceed and reformat specific messages if necessary
    switch(peer->type) {
        case NODE_TYPE_SWITCH:
        case NODE_TYPE_LEDLIGHT:
        case NODE_TYPE_HVAC :
        default:
    }
    uint8_t len = buf_in[NLINK_HDR_OFF_LEN] + NLINK_HDR_OFF_DATA;
    if (len > sizeof(peer->rx_buf)) len = sizeof(peer->rx_buf);
    memcpy(peer->rx_buf, buf_in, len);
*/

    /*
     * Mark peer as updated. Later marked peers' info
     * will be reported to UART within the idle loop.
     */
    memcpy(peer->peer_rx_buf, buf_in, NLINK_COMM_BUF_SIZE);
    peer->peer_flags |= PEER_FLAG_RX_NLINK;
}

/*
 * Mark the peer as just updated from nlink.
 * Marked peer will be send to console over UART (ha_node_ctrlcon_peers_to_sent_uart)
 */
void ha_node_ctrlcon_peer_set_rx(uint8_t remote_peer_addr)
{
    uint8_t peer_idx;

    // Check is peer already exists
    ctrlcon_peer_t *peer = &cc.peers[0];
    for (peer_idx = 0; peer_idx < ARRAY_SIZE(cc.peers); peer_idx++, peer++) {
        if (peer->remote_addr == remote_peer_addr) {
            break;
        }
    }

    if (peer_idx == ARRAY_SIZE(cc.peers)) {
        return;
    }

    peer->peer_flags |= PEER_FLAG_RX_NLINK;
}

/*
 * Process NODE info data received from console over UART
 * data header & format described by NLINK_HDR_OFF_xxx
 */
void ha_node_ctrlcon_on_rx_uart (const uint8_t *data_in, uint8_t len)
{
	uint8_t remote_peer_addr = data_in[NLINK_HDR_OFF_TO];
	uint8_t peer_idx;

	// Check is peer already exists
	ctrlcon_peer_t *peer = &cc.peers[0];
	for (peer_idx = 0; peer_idx < ARRAY_SIZE(cc.peers); peer_idx++, peer++) {
		if (peer->remote_addr == remote_peer_addr) {
			break;
		}
	}

	if (peer_idx == ARRAY_SIZE(cc.peers)) {
		return;
	}

	if ( (NLINK_HDR_OFF_DATA + data_in[NLINK_HDR_OFF_LEN] != len) ||
		 (data_in[NLINK_HDR_OFF_TYPE] != peer->type)) {
		// Bad data format
		ha_uart_resync();
		return;
	}

	memcpy(peer->peer_tx_buf, data_in, len);
	peer->peer_flags |= PEER_FLAG_RX_UART;
    cc.cc_tx_flag |= CC_TX_FLAG_PEERS;
	cc.cc_node->tx_flag = 1;
}

/*
 * Copy peer's data received from UART to nlink.io.tx_buf
 * and clear NODE_TX flag if  no more peer need to be updated.
 */

uint8_t ctrlcon_on_tx_nlink(uint8_t idx, uint8_t *buf)
{
    if (cc.cc_tx_flag & CC_TX_FLAG_NODE) {
        // Send CC node's data
        cc.cc_tx_flag &= ~CC_TX_FLAG_NODE;
        return ha_nlink_on_tx_default(idx, buf);

    } else if (cc.cc_tx_flag & CC_TX_FLAG_PEERS) {
        // Send a peer's data if exist
	    for (uint8_t peer_idx = 0; peer_idx < ARRAY_SIZE(cc.peers); peer_idx++) {
		    ctrlcon_peer_t *peer = &cc.peers[peer_idx];
		    cli();
		    if (peer->peer_flags & PEER_FLAG_RX_UART) {
			    peer->peer_flags &= ~PEER_FLAG_RX_UART;
			    uint8_t len = peer->peer_tx_buf[NLINK_HDR_OFF_LEN] + NLINK_HDR_OFF_DATA;
			    memcpy(buf, peer->peer_tx_buf, len);
			    sei();
			    return len;
		    }
		    sei();
	    }
        // No more peers to send
        cc.cc_tx_flag &= ~CC_TX_FLAG_PEERS;
        cc.cc_node->tx_flag = 0;
    }
	return 0;
}

uint8_t ha_node_ctrlcon_on_tx_uart(uint8_t *buf)
{
    // Copy to the buffer passed as an argument "ADDR TYPE LEN DATA" of
	// a node marked as updated from NLINK (PEER_FLAG_RX_NLINK)
    //
    // Prepare data to be sent to UART
    for (uint8_t peer_idx = 0; peer_idx < ARRAY_SIZE(cc.peers); peer_idx++) {
        ctrlcon_peer_t *peer = &cc.peers[peer_idx];
        cli();
            if (peer->peer_flags & PEER_FLAG_RX_NLINK) {
                peer->peer_flags &= ~PEER_FLAG_RX_NLINK;
                uint8_t len = peer->peer_rx_buf[NLINK_HDR_OFF_LEN] + NLINK_HDR_OFF_DATA;
                memcpy(buf, peer->peer_rx_buf, len);
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

    cc.cc_node = ha_nlink_node_register(CTRLCON_ADDR, NODE_TYPE_CTRLCON, ctrlcon_on_rx_nlink, ctrlcon_on_tx_nlink);

    // Send initial discovery.
    // All remote peers should respond with CTRLCON_ADDR in destination
    ha_nlink_node_send(cc.cc_node, NODE_ADDR_BC, NLINK_CMD_RD_REQ);
    cc.cc_tx_flag |= CC_TX_FLAG_NODE;
}
