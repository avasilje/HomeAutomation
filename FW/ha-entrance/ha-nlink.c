/*
 * ha_nlink.c
 *
 * Created: 5/1/2018 11:18:00 PM
 *  Author: Solit
 */ 

#include <stdint.h>
#include <string.h>
#include "ha-common.h"
#include "ha-nlink.h"

nlink_t nlink;
void ha_nlink_init()
{
    memset((uint8_t*)&nlink, 0, sizeof(nlink_t));
    for (uint8_t i = 0; i < NLINK_NODES_NUM; i ++) {
        nlink.nodes[i].idx = i;
    }
}
/*
 * Function returns index of node pending for TX
 * or -1 if nothing to send
 */
int8_t nlink_comm_check_tx()
{
    int8_t idx = 0;
    uint8_t mask = nlink.nodes_to_send_mask;
    while(mask) {
        if (mask & 1) return idx;
        idx++;
        mask >>= 1;
    }
    return -1;
}

int8_t nlink_comm_on_idle()
{
    int8_t idx = nlink_comm_check_tx();
    if ( idx < 0) return 0;

    /* 
     * There is something to send 
     */
    node_t *node = &nlink.nodes[idx];
    memcpy(nlink.comm_tx, node->tx_buf, NLINK_COMM_BUF_SIZE);
    nlink.comm_tx_rd = 0;

    nlink.nodes_to_send_mask &= ~(1 << idx);
    return 1;
}

int8_t ha_nlink_node_register(uint8_t addr, uint8_t type, uint8_t len, node_rx_cb_t on_rx_cb) 
{
    // Find next free entry
    int8_t i;
    node_t *node;
    for (i = 0; i < NLINK_NODES_NUM; i++){
        node = &nlink.nodes[i];
        if (node->addr == 0) break;             // Empty entry found
    }

    if (i == NLINK_NODES_NUM) return -1;         // Empty entry not found

    node->addr = addr;
    node->type = type;
    node->addr = addr;
    node->on_rx_cb = on_rx_cb;
    return i;
}

uint8_t nlink_node_on_rx(uint8_t addr_from, uint8_t addr_to, uint8_t cmd, uint8_t *data, uint8_t len)
{
    uint8_t i, consumed = 0;
    node_t *node = &nlink.nodes[0];
    // TODO: check most efficient loop (while + pointer; for + index; etc)
    for (i = 0; i < NLINK_NODES_NUM; i++) {

        if (node->addr == 0) break;

        if ( (node->addr < 0x92 && node->addr != addr_from) &&  // TODO: remove ugly 0x91
             (node->addr == addr_to ||               // Direct message
              node->type == NODE_TYPE_CTRLCON ||     // CTRLCON listens for all messages
              addr_to == NODE_ADDR_BC)) {            // Broadcast message
            
            // Address matched
            if (cmd == NLINK_CMD_RD_REQ) {
                // Send reply - current TX buffer
                ha_nlink_node_send(node->idx, addr_from, NLINK_CMD_RD_RESP);
            } else {
                node->rx_buf[NLINK_HDR_OFF_FROM] = addr_from;
                node->rx_buf[NLINK_HDR_OFF_TO  ] = addr_to;
                node->rx_buf[NLINK_HDR_OFF_CMD ] = cmd;
                node->rx_buf[NLINK_HDR_OFF_LEN ] = len;
                memcpy(&node->rx_buf[NLINK_HDR_OFF_DATA] , data, len);
                node->on_rx_cb(node->idx, node->rx_buf);
            }
        }
        node++;
    }

    return consumed;
}

void ha_nlink_node_send(uint8_t node_h, uint8_t addr_to, uint8_t cmd) 
{
    node_t *node = &nlink.nodes[node_h];
    node->tx_buf[NLINK_HDR_OFF_FROM] = node->addr;
    node->tx_buf[NLINK_HDR_OFF_TO  ] = addr_to;
    node->tx_buf[NLINK_HDR_OFF_CMD ] = cmd;
                 
    // Check Local nodes
    uint8_t consumed = nlink_node_on_rx(node->addr, addr_to, cmd, 
        &node->tx_buf[NLINK_HDR_OFF_DATA], node->tx_buf[NLINK_HDR_OFF_LEN]);

    // Mark as "to be send outward" if not consumed
    nlink.nodes_to_send_mask |= ((!consumed) << node_h);
}

void nlink_node_send_data(uint8_t node_h, uint8_t addr_to, uint8_t cmd, uint8_t *data, uint8_t len) 
{
    node_t *node = &nlink.nodes[node_h];
    // RD_REQ is empty request, others with data
    if (cmd != NLINK_CMD_RD_REQ && cmd != NLINK_CMD_RD_RESP) {
        memcpy(&node->tx_buf[NLINK_HDR_OFF_DATA], data, len);
        node->tx_buf[NLINK_HDR_OFF_LEN] = len;
    } 
    ha_nlink_node_send(node_h, addr_to, cmd);
}
