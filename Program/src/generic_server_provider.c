
/*
void connection_server(char *host_name, char* port, int initial_buff_size, int connection_limit,
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *app), 
    int (*onTimeOut)(void *app),
    int (*onStdIn)(void *app),
    int (*checkExit)(void *app),
    void (*onExit)(void *app),
    void *app);
*/
#include "port.h"
#include "csp.h"
#include "csp_server_provider.h"
#include "csp_conn.h"
#include "queue.h"

extern int exit_now;

void ssp_generic_connectionless_server(
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *app), 
    int (*onTimeOut)(void *app),
    int (*checkExit)(void *app),
    void (*onExit)(void *app),
    void *app) {
        
    FTP *ftp = (FTP *)app;
    csp_packet_t *packet;

    for (;;) {

        if (exit_now || checkExit(app)){
            ssp_printf("exiting server thread\n");
            break;
        }
        
        QueueHandle_t *xQueue = (QueueHandle_t*) ftp->custom_queue.queue;
        bool is_not_empty = xQueueReceive(*xQueue, packet, 5);

        if (!is_not_empty)
            onTimeOut(app);
            return;
    
        //switch ids, we do this for the sendto function, to reply
        uint8_t d_id = packet->id.dst;
        uint8_t s_id = packet->id.src;
        packet->id.dst = s_id;
        packet->id.src = d_id;
        
        uint8_t d_port = packet->id.dport;
        uint8_t s_port = packet->id.sport;
        packet->id.dport = s_port;
        packet->id.sport = d_port;
                
        if (onRecv(-1, (char *)packet->data, packet->length, NULL, packet, sizeof(csp_packet_t), app) == -1)
            ssp_printf("recv failed\n");

        csp_buffer_free(packet);
    
    }
    onExit(app);
    
}


void csp_connection_server(
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *app), 
    int (*onTimeOut)(void *app),
    int (*checkExit)(void *app),
    void (*onExit)(void *app),
    void *app)
{
	/* Pointer to current connection and packet */
	csp_packet_t *packet;
            
    FTP *ftp = (FTP *)app;

	/* Process incoming connections */
	for (;;) {

        //bit of an issue, since this can change during runtime, this exists outside of the driver
        csp_conn_t *conn = (csp_conn_t*) ftp->custom_queue.connection;
        QueueHandle_t *xQueue = (QueueHandle_t*) ftp->custom_queue.queue;
	
        bool is_not_empty = xQueueReceive(*xQueue, packet, 5);

        if (!is_not_empty)
            onTimeOut(app);
            return;
        
        if (exit_now || checkExit(app))
            break;
                    
        if (onRecv(-1, packet->data, packet_len, NULL, conn, sizeof(struct csp_conn_s), app) == -1)
                ssp_printf("recv failed\n");

        csp_buffer_free(packet);
            
        }

    onExit(app);
}