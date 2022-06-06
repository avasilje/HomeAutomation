/*
 * ha_nlink.h
 *
 * Created: 5/1/2018 11:19:38 PM
 *  Author: Solit
 *  Keep it HW agnostic!
 */

#pragma once

#define NLINK_IO_RECOVER_TIMER 64 // in nlink clocks

#ifndef NLINK_COMM_BUF_SIZE
#define NLINK_COMM_BUF_SIZE 12
#endif

typedef void (*node_rx_cb_t)(void *rx_cb_ctx, const uint8_t *buf_in);
typedef uint8_t (*node_tx_cb_t)(void *tx_cb_ctx, uint8_t *buf_out);

typedef struct node_s {

    // Registered values
    uint8_t addr;       // Node address 
    uint8_t type;       // Node type

    node_rx_cb_t on_rx_cb;
    void* on_rx_cb_ctx;

    uint8_t tx_buf[NLINK_COMM_BUF_SIZE];
    uint8_t tx_flag;
} node_t; /*  7 + BUF_SIZE */

#define NLINK_HDR_OFF_FROM 0
#define NLINK_HDR_OFF_TO   1
#define NLINK_HDR_OFF_CMD  2    // TODO: Q: Is it really necessary?
#define NLINK_HDR_OFF_TYPE 3
#define NLINK_HDR_OFF_LEN  4
#define NLINK_HDR_OFF_DATA 5

enum nlink_io_state_e {
    NLINK_IO_STATE_RECOVERING,
    NLINK_IO_STATE_IDLE,
    NLINK_IO_STATE_ACTIVE
};

typedef struct ha_nlink_io_s {
    enum nlink_io_state_e state;
    uint8_t is_rx_timer;                    // ISR - R/W
    uint8_t tx_buf[NLINK_COMM_BUF_SIZE];
    uint8_t tx_rd;                          // ISR - R/W;  IDLE - W/O
    uint8_t tx_len;
    uint8_t tx_shift_reg;

    uint8_t rx_buf[NLINK_COMM_BUF_SIZE];
    uint8_t rx_wr;
    uint8_t rx_shift_reg;
    uint8_t bit_cnt;
    uint8_t recover_timer;
    uint8_t idle_timer;
} ha_nlink_io_t; /* 10 + 32 = 42 */

typedef struct nlink_s {
    node_t nodes[NLINK_NODES_NUM];  /* 2 x 44 = 88 */
    ha_nlink_io_t io;               /*          42 */
} nlink_t;                          /*         130 */

#define NODE_TYPE_HVAC      0x10
#define NODE_TYPE_LEDLIGHT  0x20
#define NODE_TYPE_SWITCH    0x30
#define NODE_TYPE_CTRLCON   0x40
#define NODE_TYPE_PHTS      0x50
#define NODE_TYPE_ROLL      0x60

#define NODE_ADDR_BC  0xFF
#define HVAC_ADDR     0x60
#define LEDLIGHT_ADDR 0x70
#define SWITCH_ADDR   0x80
#define CTRLCON_ADDR  0x90
#define PHTS_ADDR     0xA0
#define ROLL_ADDR     0xB0

// CMD (RD_REQ/RD_RESP/WR_REQ/WR_RESP)
//     RD_REQ  - cc_node -> remote or BC (aka discovery).
//     RD_RESP - remote -> peer. Always contains ADDR_TO == ADDR_FROM from RD_REQ
//     INFO    - remote -> any (aka notification).
//

#define NLINK_CMD_RD_REQ  1
#define NLINK_CMD_RD_RESP 2
#define NLINK_CMD_INFO    3

extern void ha_nlink_init();
extern void ha_nlink_node_send(node_t *node, uint8_t addr_to, uint8_t cmd);
extern uint8_t ha_nlink_on_tx_default(uint8_t idx, uint8_t *buf);
extern node_t* ha_nlink_node_register(uint8_t addr, uint8_t type, node_rx_cb_t on_rx_cb, void* rx_cb_ctx);
extern void ha_nlink_check_rx();
extern void ha_nlink_check_tx();

/*
 * The function that need to be implemented by HW specific module
 */ 
extern void isr_nlink_io_on_timer();
extern void isr_nlink_io_on_start_edge();
extern void ha_nlink_gpio_init();

extern nlink_t nlink;