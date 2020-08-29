#ifndef SSP_GENERIC_PROVIDER_H
#define SSP_GENERIC_PROVIDER_H
#include "port.h"

void csp_generic_server(
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *app), 
    int (*onTimeOut)(void *app),
    int (*checkExit)(void *app),
    void (*onExit)(void *app),
    void *app);

void csp_generic_client(uint8_t dest_id, uint8_t dest_port, uint8_t my_port, uint32_t packet_len,
    int (*onSend)(int sfd, void *addr, uint32_t size_of_addr, void *onSendParams),
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *onRecvParams) ,
    int (*checkExit)(void *checkExitParams),
    void (*onExit)(void *params),
    void *params);

#endif 
