/*
 * ha_nlink.h
 *
 * Created: 5/1/2018 11:19:38 PM
 *  Author: Solit
 */ 

#pragma once
#define NLINK_COMM_BUF_SIZE 16

typedef void (*node_rx_cb_t)(uint8_t idx, const uint8_t *buf_in);

typedef struct node_s {
    uint8_t idx;
    
    // Registered values
    uint8_t addr;       // Node address
    uint8_t type;       // Node type
    uint8_t rx_buf[NLINK_COMM_BUF_SIZE];
    node_rx_cb_t on_rx_cb;

    uint8_t tx_buf[NLINK_COMM_BUF_SIZE];
} node_t;

#define NLINK_HDR_OFF_FROM 0
#define NLINK_HDR_OFF_TO   1
#define NLINK_HDR_OFF_CMD  2
#define NLINK_HDR_OFF_LEN  3
#define NLINK_HDR_OFF_DATA 4

#define NLINK_NODES_NUM 6       // Sum of all external and 2xlocal nodes - ctrlcon node
                                // (ctrlcon registers a node for each discovered external and local nodes)
                                // Ext nodes: switch + hvac + blinds = 3
                                // Loc nodes: ctrlcon + switch = 2
                                // Tot  = 3 + (2 * 2) - 1 = 6

typedef struct nlink_s {
    uint8_t nodes_to_send_mask;
    node_t nodes[NLINK_NODES_NUM];
    uint8_t comm_tx[NLINK_COMM_BUF_SIZE];
    uint8_t comm_tx_rd;
} nlink_t;

#define NODE_TYPE_HVAC      0x10
#define NODE_TYPE_LEDLIGHT  0x20
#define NODE_TYPE_SWITCH    0x30
#define NODE_TYPE_CTRLCON   0x40

#define NODE_ADDR_BC  0xFF
#define HVAC_ADDR     0x60
#define LEDLIGHT_ADDR 0x70
#define SWITCH_ADDR   0x80
#define CTRLCON_ADDR  0x90

// CMD (RD_REQ/RD_RESP/WR_REQ/WR_RESP)
//     RD_REQ  - cc_node -> remote or BC (aka discovery).
//     RD_RESP - remote -> peer. Always contains ADDR_TO == ADDR_FROM from RD_REQ
//     INFO    - remote ->  BC (aka notification).  
//     

#define NLINK_CMD_RD_REQ  1
#define NLINK_CMD_RD_RESP 2
#define NLINK_CMD_INFO    3

extern void ha_nlink_init();
extern void ha_nlink_node_send(uint8_t node_h, uint8_t addr_to, uint8_t cmd);
extern int8_t ha_nlink_node_register(uint8_t addr, uint8_t type, uint8_t len, node_rx_cb_t on_rx_cb);

extern nlink_t nlink;