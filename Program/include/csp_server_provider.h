/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef CSP_SERVER_H
#define CSP_SERVER_H
#include "stdint.h"

void csp_connectionless_client(uint8_t dest_id, uint8_t dest_port, uint8_t src_port, uint32_t packet_len,
    int (*onSend)(int sfd, void *addr, size_t size_of_addr, void *onSendParams),
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *onRecvParams) ,
    int (*checkExit)(void *checkExitParams),
    void (*onExit)(void *params),
    void *params);

void csp_connectionless_server(uint8_t my_port, uint32_t packet_len, uint32_t time_out,
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *other), 
    int (*onTimeOut)(void *other),
    int (*checkExit)(void *other),
    void (*onExit)(void *other),
    void *other);

void csp_connection_server(uint8_t my_port, uint32_t packet_len, uint32_t time_out,
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *other), 
    int (*onTimeOut)(void *other),
    int (*checkExit)(void *other),
    void (*onExit)(void *other),
    void *other);

void csp_connection_client(uint8_t dest_id, uint8_t dest_port, uint8_t my_port, uint32_t packet_len, uint32_t time_out, void*lock,
    int (*onSend)(int sfd, void *addr, size_t size_of_addr, void *onSendParams),
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *onRecvParams) ,
    int (*checkExit)(void *checkExitParams),
    void (*onExit)(void *params),
    void *params);
int csp_custom_ftp_ping(uint32_t dest_id, uint32_t port);
#endif
