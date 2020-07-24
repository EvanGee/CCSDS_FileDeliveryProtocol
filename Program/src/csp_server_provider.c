/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/

#include "port.h"
#include "csp.h"
#include "csp_server_provider.h"
#include "csp_conn.h"


int exit_now;

/*------------------------------------------------------------------------------
                                    
                                    CSP STUFF!

------------------------------------------------------------------------------*/

//https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpclient.c
void csp_connectionless_client(uint8_t dest_id, uint8_t dest_port, uint8_t src_port, uint32_t packet_len,
    int (*onSend)(int sfd, void *addr, uint32_t size_of_addr, void *params),
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *params) ,
    int (*checkExit)(void *params),
    void (*onExit)(void *params),
    void *params) 
{

    int err = 0;
    uint8_t src_id = csp_get_address();

    //csp_socket_t *socket = csp_socket(CSP_SO_XTEAREQ | CSP_SO_HMACREQ | CSP_SO_CRC32REQ);
    csp_socket_t *soc = csp_socket(CSP_SO_CONN_LESS);
    if (soc == NULL) {
        ssp_error("ERROR: csp socket queue empty\n");
        return; 
    }

    err = csp_bind(soc, src_port);
    if (err < 0) {
        ssp_error("ERROR: couldn't bind csp\n");
        return; 
    }

    csp_packet_t *packet_sending;
    csp_packet_t *packet_recieved;

    if (csp_buffer_remaining() != 0) {

        packet_sending = csp_buffer_get(1);
        packet_sending->id.dst = dest_id;
        packet_sending->id.dport = dest_port;
        packet_sending->id.src = src_id;
        packet_sending->id.sport = src_port;
        
    }
    else {
        ssp_error("couldn't get new packet for sending!\n");
        exit_now = 1;
    }
    char buff[packet_len];
    memset(buff, 0, packet_len);

    for (;;) {

        if (exit_now || checkExit(params)){
            ssp_printf("exiting client thread\n");
            break;
        }

        onSend(-1, packet_sending, sizeof(csp_packet_t), params);

        packet_recieved = csp_recvfrom(soc, 10);
        //timout
        if (packet_recieved == NULL) {
            continue;
        }  
        else {
            
            memcpy(buff, packet_recieved->data, packet_len);
             
            if (onRecv(-1, buff, packet_len, NULL, packet_recieved, sizeof(csp_packet_t), params) == -1)
                ssp_printf("recv failed\n");

            csp_buffer_free(packet_recieved);
        }
        
    }
    csp_buffer_free(packet_sending);
}



void csp_connectionless_server(uint8_t my_port, uint32_t packet_len,
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *other), 
    int (*onTimeOut)(void *other),
    int (*onStdIn)(void *other),
    int (*checkExit)(void *other),
    void (*onExit)(void *other),
    void *other)
{

    //csp_socket_t *socket = csp_socket(CSP_SO_XTEAREQ | CSP_SO_HMACREQ | CSP_SO_CRC32REQ);
    csp_socket_t *soc = csp_socket(CSP_SO_CONN_LESS);
    
    int err = csp_bind(soc, my_port);

    if (err < 0) {
        ssp_error("ERROR: couldn't bind csp\n");
        return; 
    }

    for (;;) {

        if (exit_now || checkExit(other)){
            ssp_printf("exiting server thread\n");
            break;
        }
    
        csp_packet_t *packet = csp_recvfrom(soc, 10);
        //timeout
        if (packet == NULL) {
            onTimeOut(other);
        }
        else {

            //switch ids, we do this for the sendto function, to reply
            uint8_t d_id = packet->id.dst;
            uint8_t s_id = packet->id.src;
            packet->id.dst = s_id;
            packet->id.src = d_id;
            
            uint8_t d_port = packet->id.dport;
            uint8_t s_port = packet->id.sport;
            packet->id.dport = s_port;
            packet->id.sport = d_port;
                  
            if (onRecv(-1, (char *)packet->data, packet->length, NULL, packet, sizeof(csp_packet_t), other) == -1)
                ssp_printf("recv failed\n");

            csp_buffer_free(packet);
        }
        
    }
}

void csp_connection_server(uint8_t my_port, uint32_t packet_len,
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *other), 
    int (*onTimeOut)(void *other),
    int (*onStdIn)(void *other),
    int (*checkExit)(void *other),
    void (*onExit)(void *other),
    void *other)
{

    //csp_socket_t *socket = csp_socket(CSP_SO_XTEAREQ | CSP_SO_HMACREQ | CSP_SO_CRC32REQ);

	/* Create socket without any socket options */
	csp_socket_t *sock = csp_socket(CSP_SO_NONE);

    uint8_t src_id = csp_get_address();
    ssp_printf("CSP ID: %d\n", src_id);

	/* Bind all ports to socket */
	csp_bind(sock, my_port);

	/* Create 10 connections backlog queue */
	csp_listen(sock, 10);

	/* Pointer to current connection and packet */
	csp_conn_t *conn;
	csp_packet_t *packet;

    char buff[packet_len];
    memset(buff, 0, packet_len);

	/* Process incoming connections */
	for (;;) {

        if (exit_now || checkExit(other)){
            ssp_printf("exiting serv thread\n");
            break;
        }
        
		/* Wait for connection, 1000 ms timeout */
		if ((conn = csp_accept(sock, 10)) == NULL) {
            onTimeOut(other);
            continue;
        }

        for (;;) {
            
            if (exit_now || checkExit(other))
                break;
        
            onTimeOut(other);
            
            while ((packet = csp_read(conn, 100)) != NULL) {                
                memcpy(buff, (char *)packet->data, packet_len);
                
                if (onRecv(-1, buff, packet_len, NULL, conn, sizeof(struct csp_conn_s), other) == -1)
                        ssp_printf("recv failed\n");

                csp_buffer_free(packet);
            }
        }
        csp_close(conn);
        onExit(other);

	}
}


void csp_connection_client(uint8_t dest_id, uint8_t dest_port, uint8_t my_port, uint32_t packet_len,
    int (*onSend)(int sfd, void *addr, uint32_t size_of_addr, void *onSendParams),
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *onRecvParams) ,
    int (*checkExit)(void *checkExitParams),
    void (*onExit)(void *params),
    void *params)
{

	csp_packet_t * packet;
	csp_conn_t * conn;


    char buff[packet_len];
    memset(buff, 0, packet_len);

	while (1) {

        if (exit_now || checkExit(params)){
            ssp_printf("exiting client thread\n");
            break;
        }
        
		/* Connect to host HOST, port PORT with regular UDP-like protocol and 1000 ms timeout */
		conn = csp_connect(CSP_PRIO_NORM, dest_id, dest_port, 1000, CSP_CONNECTION_SO);
		if (conn == NULL) {
			/* Connect failed */
			ssp_printf("Connection failed\n");
			return;
		}

        for (;;) {
            if (exit_now || checkExit(params)){
                ssp_printf("exiting client thread\n");
                break;
            }
            onSend(-1, conn, sizeof(conn), params);
            while ((packet = csp_read(conn, 10)) != NULL) {
                            
                memcpy(buff, (char *)packet->data, packet_len);

                if (onRecv(-1, buff, packet_len, NULL, conn, sizeof(struct csp_conn_s), params) == -1)
                        ssp_printf("recv failed\n");

                csp_buffer_free(packet);

            }
        }
        
		/* Close connection */
		csp_close(conn);
        onExit(params);
	}
}