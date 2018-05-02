/*
 * ha_ndoe_ctrlcon.c
 *
 * Created: 5/1/2018 11:26:18 PM
 *  Author: Solit
 */ 
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
    uint8_t flags;          // RX flag NODE_FLAG_xxx
    uint8_t remote_addr;
    uint8_t type;
} ctrlcon_peer_t;

#define PEER_FLAG_VALID 1      // for sanity purposes
#define PEER_FLAG_RX    2      // Set on update from nlink, cleared upon processed by ctrlcon UART

typedef struct ctrlcon_s {
    uint8_t cc_node_h;
    ctrlcon_peer_t peers[NLINK_NODES_NUM];  // used number off peers = (NLINK_NODES_NUM - LOCAL_NODES_NUM - 1) 
                                            // = EXTERNAL_NODES_NUM + LOCAL_NODES_NUM - 1
                                            // Map peers=>nodes as one-to-one for simplicity, but waste some memory
    uint8_t last_reg_addr;
} ctrlcon_t;

ctrlcon_t cc;

void dummy_on_peer_rx(uint8_t idx, const uint8_t *buf_in)
{
    FATAL_TRAP(__LINE__);
}

void ctrlcon_on_peer_rx(uint8_t idx, const uint8_t *buf_in)
{
    cc.peers[idx].flags |= PEER_FLAG_RX;
    ha_uart_enable_tx();
}

void ctrlcon_on_rx(uint8_t idx, const uint8_t *buf_in) 
{
    uint8_t peer_idx;

    // Check is peer already exists
    for (peer_idx = 0; peer_idx < NLINK_NODES_NUM; peer_idx++) {
        if (cc.peers[peer_idx].remote_addr == buf_in[NLINK_HDR_OFF_FROM]) {
            break;
        }
    }

    if (peer_idx == NLINK_NODES_NUM) {
        // New remote peer - register. In fact we don't need yet another node, but just set of RX/TX buffers
        // TODO: get rid of node registration. CTRLCON should maintains its own RX/TX buffers for each peer.
        peer_idx = ha_nlink_node_register(
            cc.last_reg_addr+1,             /* Peer node addr */
            buf_in[NLINK_HDR_OFF_DATA + 0], /* Type */
            16,                             /* Len */
            dummy_on_peer_rx);              /* Dummy. Peer should never get messages directly  */
                                            /* from nlink interface, so it's dummy */

        if (peer_idx < 0) {
            FATAL_TRAP(__LINE__);
        }
        // New node registered - save peers data
        cc.last_reg_addr++;
        cc.peers[peer_idx].idx = peer_idx; 
        cc.peers[peer_idx].flags = PEER_FLAG_VALID;
        cc.peers[peer_idx].remote_addr = buf_in[NLINK_HDR_OFF_FROM];
        cc.peers[peer_idx].type = buf_in[NLINK_HDR_OFF_DATA + 0];
    }

    // Copy data from received packet to peer node and process it as just received from peer
    memcpy(&nlink.nodes[peer_idx].rx_buf, buf_in, NLINK_COMM_BUF_SIZE);
    ctrlcon_on_peer_rx(peer_idx, NULL); 
}

uint8_t ha_node_ctrlcon_to_sent(uint8_t *buf) 
{
    uint8_t peer_idx;
    // Check is peer already exists
    for (peer_idx = 0; peer_idx < ARRAY_SIZE(cc.peers); peer_idx++) {

        if (cc.peers[peer_idx].flags & PEER_FLAG_RX) {
            cc.peers[peer_idx].flags &= ~PEER_FLAG_RX;
            uint8_t *buf_out = nlink.nodes[peer_idx].rx_buf;
            uint8_t len = buf_out[NLINK_HDR_OFF_LEN] + NLINK_HDR_OFF_DATA;
            memcpy(buf, nlink.nodes[peer_idx].rx_buf, len);
            return len;
        }
    }
    return 0;
}

void ha_node_ctrlcon_init() 
{
    memset((uint8_t*)&cc, 0, sizeof(ctrlcon_t));

    cc.last_reg_addr = CTRLCON_ADDR;
    uint8_t my_node = ha_nlink_node_register(cc.last_reg_addr++, NODE_TYPE_CTRLCON, NLINK_COMM_BUF_SIZE, ctrlcon_on_rx);
    cc.cc_node_h = my_node;

    // Send initial discovery.
    // All remote peers should respond with CTRLCON_ADDR in destination
    ha_nlink_node_send(my_node, NODE_ADDR_BC, NLINK_CMD_RD_REQ);
}
