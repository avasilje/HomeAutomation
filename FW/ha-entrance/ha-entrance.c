#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

/*
 * node-entrance.c
 *  contains node-switch and node-cc
 *
 * Created: 08/04/2018
 *  Author: Solit
 */ 

#include "ha-entrance.h"

HW_INFO gt_hw_info = {0, 2, "AV HA Node"};
PF_PVOID gpf_action_func;

extern T_ACTION gta_action_table[];

uint16_t gus_trap_line;

#define INT_FATAL_TRAP(us_line_num)     \
    do {                                \
        gus_trap_line = us_line_num;    \
        while(1);                       \
    } while(1)

void FATAL_TRAP (uint16_t us_line_num) { 
    gus_trap_line = us_line_num;
    while(1);
}

#define MSG_BUFF_SIZE 32
volatile uint8_t guc_in_msg_idx;
uint8_t guca_in_msg_buff[MSG_BUFF_SIZE];

volatile uint8_t guc_out_msg_wr_idx; 
volatile uint8_t guc_out_msg_rd_idx;
uint8_t guca_out_msg_buff[MSG_BUFF_SIZE];


#define IN_SW_0_MASK (1 << PINB7)
#define IN_SW_1_MASK (1 << PINB6)
#define IN_SW_2_MASK (1 << PINB0)
#define IN_SW_3_MASK (1 << PINB1)

#define IN_SW_0 PINB7
#define IN_SW_1 PINB6
#define IN_SW_2 PINB0
#define IN_SW_3 PINB1
#define IN_SW_MASK \
    ((1 << IN_SW_0)|\
     (1 << IN_SW_1)|\
     (1 << IN_SW_2)|\
     (1 << IN_SW_3))

#define OUT_SW0_0 PINB4
#define OUT_SW0_1 PINB3
#define OUT_SW0_2 PINB2

#define OUT_SW0_0_MASK (1 << OUT_SW0_0)
#define OUT_SW0_1_MASK (1 << OUT_SW0_1)
#define OUT_SW0_2_MASK (1 << OUT_SW0_2)

#define OUT_SW0_MASK \
    ((1 << OUT_SW0_0)|\
     (1 << OUT_SW0_1)|\
     (1 << OUT_SW0_2))

#define OUT_SW1_3    PIND5
#define OUT_SW1_3_MASK (1 << PIND5)
#define OUT_SW1_MASK (OUT_SW1_3_MASK)

#define ENABLE_PCINT  GIMSK |= (1 << PCIE)
#define DISABLE_PCINT  GIMSK &= ~(1 << PCIE)

#define TIMER_CNT_SCALER 0

#ifndef DBG_EN
#define DBG_EN 0
#endif

#if DBG_EN
#define DBG_VAR_NUM 24
uint16_t dbg_p[DBG_VAR_NUM];
uint8_t dbg_idx = 0;
#endif

static void init_uart()
{
    // Init UART to 0.5Mbps 8N1

    // Set baud rate. UBRR = Fosc/16/BAUD - 1
    // Fosc = 20MHz, UBRR = 20*10^6/16/250000 - 1 = 4
    UBRRH = 0;
    UBRRL = 4;

    // Set frame format: 8data, 1stop bit, no parity */
    UCSRC = 
        _BV(UCSZ0) |      // 8 data bits
        _BV(UCSZ1);      // 8 data bits

    // Enable receiver and transmitter
    UCSRB =
        // (0<<TXCIE0)|     // TX Interrupt disable
        _BV(RXCIE)|     // RX Interrupt enable
        _BV(RXEN) |     // Receiver enabled
        _BV(TXEN);      // Transmitter enabled
}

static void init_gpio(){
    DDRB = 0;

    // IN switches - pulled up inputs
    DDRB &= ~IN_SW_MASK; 
    PORTB |= IN_SW_MASK; 

    // Out switches - output 0
    DDRB |= OUT_SW0_MASK; 
    PORTB &= ~OUT_SW0_MASK;

    DDRD |= OUT_SW1_MASK; 
    PORTD &= ~OUT_SW1_MASK;

    PCMSK = IN_SW_MASK;
    ENABLE_PCINT;

    EIFR &= ~(1 << PCIF);
}

void send_out_msg(uint8_t uc_len)
{
    uint8_t uc_i;

    for (uc_i = 0; uc_i < uc_len; uc_i++) {
        loop_until_bit_is_clear(UCSRA, UDRE);
        UDR = guca_out_msg_buff[uc_i];
    }
}


static void command_lookup()
{
    T_ACTION    *pt_action;
    PF_PVOID    pf_func;

    uint8_t uc_act_cmd;
    uint8_t uc_msg_cmd;
    
    // check is command valid
    // MARK = "AV", read pointer must be set on buffer start
    if ( guca_in_msg_buff[0] != 0x41)
        FATAL_TRAP(__LINE__);

    if ( guca_in_msg_buff[1] != 0x86)
        FATAL_TRAP(__LINE__);

    uc_msg_cmd = guca_in_msg_buff[2];

    // --------------------------------------------------
    // --- Find received command in actions' table
    // --------------------------------------------------
    pt_action = gta_action_table;

    // Check functions table signature
    pf_func = (PF_PVOID)pgm_read_word_near(&pt_action->pf_func);
    if (pf_func != (PF_PVOID)0xFEED)
        FATAL_TRAP(__LINE__);

    // Find action in table
    do 
    {
        pt_action ++;
        uc_act_cmd = pgm_read_byte_near(&pt_action->uc_cmd);

        // TRAP if unknown command received (end of table reached)
        if (uc_act_cmd == 0xFF) 
            FATAL_TRAP(__LINE__);

        if (uc_msg_cmd == uc_act_cmd){
            // get functor from table
            gpf_action_func = (PF_PVOID)pgm_read_word_near(&pt_action->pf_func);
            break;
        }
    } while(1); // End of action table scan

    gpf_action_func();

    // Clear RX buffer index in order to receive next message
    guc_in_msg_idx = 0;

    return;
}

int main(void)
{
    init_gpio();
    
    init_uart();

    sei();

    while(1) 
    {
        // Get number of bytes received (4 bytes at least)
        if (guc_in_msg_idx >= 4)
        {
            // Init transmitter's buffer
            guc_out_msg_wr_idx = 0;
            guc_out_msg_rd_idx = 0;

            // Find and execute command from the ACTIONS TABLE
            command_lookup();
        }

        // Is something to send
        if (guc_out_msg_wr_idx > guc_out_msg_rd_idx)
        {
            // check is current byte is send completely
            if (UCSRA & (1 << UDRE))
            { // TX FIFIO is empty. 

                UDR = guca_out_msg_buff[guc_out_msg_rd_idx];
                UCSRA  |= (1 << TXC); // Clear transmit complete flag
                guc_out_msg_rd_idx ++;
            }
        }
    } 
    cli();

}

ISR(PCINT_vect) {

}

// Interrupt triggered every 256 timer clocks and count periods
ISR(TIMER0_OVF_vect) {

}

#if 0
            // Check previous transaction is completed (TXC: USART Transmit Complete)
            if (UCSR0A & (1 << TXC0))
            { // TX FIFIO is empty. Write 16bit ADC sample to UART TX FIFO
                UDR0 = uc_data_l;
                UDR0 = uc_data_h;
                UCSR0A  |= (1 << TXC0); // Clear transmit complete flag
            }
#endif

ISR(USART_RX_vect) {

    uint8_t uc_status;
    uint8_t uc_data;

    // get RX status, trap if something wrong
    uc_status = UCSRA;
    uc_data = UDR;

    // Check Frame Error (FE0) and DATA overflow (DOR0)
    if ( uc_status & (_BV(FE) | _BV(DOR)) )
        INT_FATAL_TRAP(__LINE__);

    if ( guc_in_msg_idx == MSG_BUFF_SIZE )
        INT_FATAL_TRAP(__LINE__);

    guca_in_msg_buff[guc_in_msg_idx++] = uc_data;
  
}

void action_signature()
{

    uint8_t uc_i;

    // Command format
    // 0x41 0x86 0x21  0xLL    0xAA      
    // MARK      CMD   Length  Address   

    // Response format
    // 0x41 0x86 0x11    0xLL    0xMM    0xMM    0xCC
    // MARK      Length  Major   Minor   String name

    // Check is message length available, wait if not yet
    while(guc_in_msg_idx < 5);

    // Write header & length
    guca_out_msg_buff[0] = 0x41;
    guca_out_msg_buff[1] = 0x86;

    guca_out_msg_buff[2] = 0x11;
    guca_out_msg_buff[3] = SIGN_LEN + 2;

    // Write version number
    guca_out_msg_buff[4] = gt_hw_info.uc_ver_maj;
    guca_out_msg_buff[5] = gt_hw_info.uc_ver_min;

    // Write signature
    for(uc_i = 0; uc_i < SIGN_LEN; uc_i++)
    {
        guca_out_msg_buff[uc_i+6] = gt_hw_info.ca_signature[uc_i];
    }

    // Check no more input data received
    if (guc_in_msg_idx != 2)
        FATAL_TRAP(__LINE__);

    guc_out_msg_wr_idx = SIGN_LEN + 6;

    // End of action_signature
    return;
}

void action_node_info() 
{
#define NODE_ADDR_BC -1
    // Command format
    // 0x41 0x86 0x21  0xLL    0xAA      
    // MARK      CMD   Length  Address   
    
    // Response format
    // 0x41 0x86 0x21  0xLL    | 0xAA      0xTT   0xDD         | 0xAA      0xTT   0xDD      |
    // MARK      CMD   Length  | Address   Type   node info    | Address   Type   node info |  

    // Check is message address is available, wait if not yet
    while(guc_in_msg_idx < 6);

    // Get node address
    uint8_t req_addr = guca_in_msg_buff[4];

    node_get_info(addr);
#if 0

    // If our own, then prepare response
    // Retransmit if addr == BC
    if (addr == NODE_ADDR_BC || node_is_own_addr(addr)) {
        
        if (addr == NODE_ADDR_BC) {
            // Copy to NODE_IF
        }
        // Respond with node 
        uint8_t wr_idx = 4
        
        while( node_responde(req_addr, &guca_out_msg_buff[]) ;){

        }
            
    }
    
    uint8_t data_len = guca_out_msg_buff[uc_i+2] = uc_feedback;
    guc_out_msg_wr_idx = uc_i + 2;
#endif
}

T_ACTION gta_action_table[4] = {
    { "mark", 0x00, (void*)0xFEED        },    // Table signature
    { "sign", 0x11, action_signature     },    
    { "ninf", 0x21, action_node_info     },    
    { ""    , 0xFF, NULL                 }     // End of table
};

void node_get_info(uint8_t addr) {

     // for all registered nodes
     {
        
        // If node addr == addr || addr == BC
            {
                // mark node as info requested
            }
     }
}

typedef struct node_s {
    uint8_t idx;
    
    // Registered values
    uint8_t addr;       // Node address
    uint8_t type;       // Node type
    uint8_t rx_buf[8];  // ADDR_FROM LEN DATA
    void (on_rx*)(uint8_t idx);    // 

    uint8_t tx_buf[8];  // ADDR_TO LEN DATA
} node_t;

typedef struct nlink_s {
    node_t nodes[8];
} nlink_t;

// HVAC DATA
//     TYPE(HVAC) CMD(RD_REQ/RD_RESP/WR_REQ/WR_RESP) HEATER(%), VALVE(ON/OFF), MOTOR (I/II/OFF), TEMP, PRES, HUM.
//     
//     RD_REQ  - cc_node -> remote or BC (aka discovery).
//     RD_RESP - remote -> peer. Always contains ADDR_TO == ADDR_FROM from RD_REQ
//     INFO    - remote ->  BC (aka notification).  
//     
//     

#define NLINK_CMD_RD_REQ
typedef struct ctrlcon_s{
    uint8_t peers[16];       // Array of discovered nodes' indices. Filled at register. Used while unload to UART toward UI
    uint8_t peers_flags[16];       // RX flag NODE_FLAG_xxx
    uint8_t peers_remote_addr[16]; 
    uint8_t last_reg_addr;
} ctrlcon_t;

#define NODE_TYPE_HVAC 1
#define NODE_TYPE_LEDLIGHT 2
#define CTRLCON_ADDR 0x80
ctrlcon_t cc;
nlink_t nlink;

uint8_t nlink_node_register(uint8_t addr, uint8_t type, uint8_t len, void (on_rx*)(uint8_t idx)) 
{
    // Find next free entry
    // save addr type, len and on_rx() callback
    // mark entry as valid
    // return entry idx
}

#define PEER_FLAG_VALID 1      // for sanity purposes
#define PEER_FLAG_RX    2      // Set on update from nlink, cleared upon processed by ctrlcon UART

void ctrlcon_on_peer_rx(uint8_t idx)
{
    cc.peers_flags[idx] |= PEER_FLAG_RX;
    // enable CC UART_TX_EMPTY interrupt. The interrupt will unload all peer nodes marked as RX
}
void ctrlcon_on_rx(uint8_t idx) 
{
    // Any read response expected here
    // Sanity 
    // Ignore if CMD != RD_RESP
    // Find node in node_remote_addr[] by ADDR_FROM
    // If node is new, then 
    // 
    
    uint8_t *buf_in = &nlink.nodes[idx].rx_buf; // ADDR_FROM LEN DATA
    
    uint8_t peer_idx = nlink_register_node(cc.last_reg_addr++, buf_in[2] /*type*/, buf_in[1] /*len*/, ctrlcon_on_peer_rx);
    cc.peers_flags[peer_idx] = PEER_FLAG_VALID;
    cc.peers_remote_addr[peer_idx] = data[0];

    uint8_t *peer_buf_in = &nlink.nodes[peer_idx].rx_buf;

    // Copy data from received packet to newly created node and mark it as process it as just received
    memcpy(peer_buf_in, buf_in, NODE_RX_BUFF_SIZE);
    ctrlcon_on_peer_rx(peer_idx)
}

void ctrlcon_init() 
{
    cc.last_reg_addr = CTRLCON_ADDR;
    nlink_node_register(cc.last_reg_addr++, CTRLCON_TYPE, NODE_MAX_LEN, ctrlcon_on_rx);

    nlink_node_send()
    // All remote peers should respond with CTRLCON_ADDR in destination
}

