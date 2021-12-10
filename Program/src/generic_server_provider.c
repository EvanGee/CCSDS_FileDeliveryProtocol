
//strong dependency for FreeRTOS here
#include "generic_server_provider.h"

#ifdef FREE_RTOS_PORT 
#include "FreeRTOS.h"
#include "queue.h"
#include "port.h"
#include "csp/csp.h"

QueueHandle_t xQueueFtpServerReceive;
QueueHandle_t xQueueFtpClientReceive;
QueueHandle_t sendQueue;


void _init_queues(int server_size, int client_size){
    if (xQueueFtpServerReceive == NULL)
        xQueueFtpServerReceive = xQueueCreate(server_size, sizeof (csp_packet_t*));
    if (xQueueFtpClientReceive == NULL) 
        xQueueFtpClientReceive = xQueueCreate(client_size, sizeof (csp_packet_t*));
}

void csp_generic_server(
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *app), 
    int (*onTimeOut)(void *app),
    int (*checkExit)(void *app),
    void (*onExit)(void *app),
    void *app)
{
    csp_packet_t *packet;
    _init_queues(10, 10);

	for (;;) {
	
        bool is_not_empty = xQueueReceive(xQueueFtpServerReceive, packet, 100);

        if (!is_not_empty) {
            onTimeOut(app);
            return;
        }

        if (get_exit() || checkExit(app))
            break;

        uint32_t len = packet->length;
        if (onRecv(-1, (char *) packet->data, packet->length, &len, packet, sizeof(csp_packet_t), app) == -1)
            ssp_printf("recv failed\n");

        csp_buffer_free(packet);
            
        }

    onExit(app);
}




void csp_generic_client(uint8_t dest_id, uint8_t dest_port, uint8_t my_port, uint32_t packet_len,
    int (*onSend)(int sfd, void *addr, uint32_t size_of_addr, void *onSendParams),
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *onRecvParams) ,
    int (*checkExit)(void *checkExitParams),
    void (*onExit)(void *params),
    void *params)
{

	csp_packet_t packet_send;
    packet_send.id.dst=dest_id;
    packet_send.id.src=csp_get_address();
    packet_send.id.dport=dest_port;
    packet_send.id.sport=my_port;
    
    csp_packet_t *packet_recv;
    
    for (;;) {
        if (get_exit() || checkExit(params)){
            ssp_printf("exiting client thread\n");
            break;
        }

        onSend(-1, &packet_send, sizeof(csp_packet_t), params);
                
        bool is_not_empty = xQueueReceive(xQueueFtpServerReceive, packet_recv, 100);

        if (!is_not_empty)
            return;

        if (onRecv(-1, (char *) packet_recv->data, packet_recv->length, NULL, packet_recv, sizeof(csp_packet_t), params) == -1)
            ssp_printf("recv failed\n");

        csp_buffer_free(packet_recv);

    }

    onExit(params);
}
#else
//for compiling without freeRTOS, but these functions do nothing.
void csp_generic_server(
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *app), 
    int (*onTimeOut)(void *app),
    int (*checkExit)(void *app),
    void (*onExit)(void *app),
    void *app){
        ssp_printf("free Rtos not defined, can't do shit\n");
    }

void csp_generic_client(uint8_t dest_id, uint8_t dest_port, uint8_t my_port, uint32_t packet_len,
    int (*onSend)(int sfd, void *addr, uint32_t size_of_addr, void *onSendParams),
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *onRecvParams) ,
    int (*checkExit)(void *checkExitParams),
    void (*onExit)(void *params),
    void *params){
        ssp_printf("free Rtos not defined, can't do shit\n");
    }
#endif
