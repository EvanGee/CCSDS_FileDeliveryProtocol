/*------------------------------------------------------------------------------
CMPT-361-AS50(1) - 2017 Fall - Introduction to Networks
Assignment #2
Evan Giese 1689223

This is my file for server.c. It develops a udp server for select.
------------------------------------------------------------------------------*/

#include "csp.h"
#include "csp_server_provider.h"
#include "port.h"
static int exit_now;

/*------------------------------------------------------------------------------
                                    
                                    CSP STUFF!

------------------------------------------------------------------------------*/

//#ifdef CSP_NETWORK

//https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpclient.c
void csp_connectionless_client(uint8_t dest_id, uint8_t dest_port, uint8_t src_port,
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


    err = csp_bind(soc, CSP_ANY);
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

    for (;;) {

        if (exit_now || checkExit(params)){
            ssp_printf("exiting server thread\n");
            break;
        }

        onSend(-1, packet_sending, sizeof(*packet_sending), params);
        
        packet_recieved = csp_recvfrom(soc, 10);
        
        //timout
        if (packet_recieved == NULL)
            continue;
    
        else {
            ssp_printf("CLIENT DATA Length: %d\n", packet_recieved->length);
            if (onRecv(-1, (char *)packet_recieved->data, packet_recieved->length, NULL, packet_recieved, sizeof(packet_recieved), params) == -1)
                    ssp_printf("recv failed\n");

            csp_buffer_free(packet_recieved);
        }
        
    }
    csp_buffer_free(packet_sending);
}



void csp_connectionless_server(uint8_t my_port,
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
        
        //timout
        if (packet == NULL) {
            onTimeOut(other);
        }
        else {

            if (onRecv(-1, (char *)packet->data, packet->length, NULL, packet, sizeof(packet), other) == -1)
                    ssp_printf("recv failed\n");

            csp_buffer_free(packet);
        }
        
    }
}

//This doesn't work because it can't have multiple connections, maybe revisit?
void csp_connection_server(uint8_t my_port,
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *other), 
    int (*onTimeOut)(void *other),
    int (*onStdIn)(void *other),
    int (*checkExit)(void *other),
    void (*onExit)(void *other),
    void *other)
{

	/* Create socket without any socket options */
	csp_socket_t *sock = csp_socket(CSP_SO_NONE);

	/* Bind all ports to socket */
	csp_bind(sock, my_port);

	/* Create 10 connections backlog queue */
	csp_listen(sock, 10);

	/* Pointer to current connection and packet */
	csp_conn_t *conn;
	csp_packet_t *packet;

	/* Process incoming connections */
	for (;;) {

        if (exit_now || checkExit(other)){
            ssp_printf("exiting serv thread\n");
            break;
        }
        
		/* Wait for connection, 1000 ms timeout */
		if ((conn = csp_accept(sock, 1000)) == NULL) {
            onTimeOut(other);
            continue;
        }

        for (;;) {
            
            if (exit_now || checkExit(other))
                break;

            onTimeOut(other);

            while ((packet = csp_read(conn, 100)) != NULL) {
                if (onRecv(-1, (char *)packet->data, packet->length, NULL, conn, sizeof(conn), other) == -1)
                        ssp_printf("recv failed\n");

                csp_buffer_free(packet);

            }
        }

        csp_close(conn);
        onExit(other);

	}

    /* Close current connection, and handle next */
	csp_close(conn);
}


void csp_connection_client(uint8_t dest_id, uint8_t dest_port,
    int (*onSend)(int sfd, void *addr, uint32_t size_of_addr, void *onSendParams),
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *onRecvParams) ,
    int (*checkExit)(void *checkExitParams),
    void (*onExit)(void *params),
    void *params)
{

	csp_packet_t * packet;
	csp_conn_t * conn;


	while (1) {


        if (exit_now || checkExit(params)){
            ssp_printf("exiting client thread\n");
            break;
        }
        
		/* Connect to host HOST, port PORT with regular UDP-like protocol and 1000 ms timeout */
		conn = csp_connect(CSP_PRIO_NORM, dest_id, dest_port, 1000, CSP_O_NONE);
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

            while ((packet = csp_read(conn, 100)) != NULL) {
                if (onRecv(-1, (char *)packet->data, packet->length, NULL, conn, sizeof(conn), params) == -1)
                        ssp_printf("recv failed\n");

                csp_buffer_free(packet);

            }
        }
        
		/* Close connection */
		csp_close(conn);
        onExit(params);
	}
}

