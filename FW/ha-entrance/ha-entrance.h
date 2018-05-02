/*
 * node_entrance.h
 *
 * Created: 4/8/2018 9:30:38 AM
 *  Author: Solit
 */ 
#pragma once

typedef void (*PF_PVOID)();

#define SIGN_LEN 16

typedef struct {
    uint8_t  uc_ver_maj;
    uint8_t  uc_ver_min;
    char     ca_signature[SIGN_LEN];
} HW_INFO;

typedef struct action_tag {
    char    ca_name[4];
    uint8_t uc_cmd;
    void    (*pf_func)(void);
} T_ACTION;

extern void ha_uart_enable_tx();


