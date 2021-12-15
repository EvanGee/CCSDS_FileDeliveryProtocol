
/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/

#include "port.h"
#include "csp/csp.h"
#include "csp_server_provider.h"
#include "csp_conn.h"


/*------------------------------------------------------------------------------
                                    
                                    CSP STUFF!

------------------------------------------------------------------------------*/

//https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpclient.c
void csp_connectionless_client(uint8_t dest_id, uint8_t dest_port, uint8_t src_port, uint32_t packet_len,
    int (*onSend)(int sfd, void *addr, size_t size_of_addr, void *params),
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
        set_exit();
    }
    char *buff = ssp_alloc(sizeof(char), packet_len);
    memset(buff, 0, packet_len);

    for (;;) {

        if (get_exit() || checkExit(params)){
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
    onExit(params);
}



void csp_connectionless_server(uint8_t my_port, uint32_t packet_len, uint32_t time_out,
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *other), 
    int (*onTimeOut)(void *other),
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

        if (get_exit() || checkExit(other)){
            ssp_printf("exiting server thread\n");
            break;
        }
    
        csp_packet_t *packet = csp_recvfrom(soc, time_out);
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

int csp_custom_ftp_ping(uint32_t dest_id, uint32_t port){

    char buff[255];
    memset(buff, 0, 255);

    csp_packet_t * packet;
    csp_conn_t * conn;
    /* Connect to host HOST, port PORT with regular UDP-like protocol and 1000 ms timeout */
    conn = csp_connect(CSP_PRIO_NORM, dest_id, port, 1000, CSP_SO_NONE);
    if (conn == NULL) {
        /* Connect failed */
        ssp_printf("Connection failed\n");
        return -1;
    }

    ssp_printf("connection established, sending ping to id %d port %d \n", dest_id, 1);
    packet = csp_buffer_get(100);
    if (packet == NULL) {
        ssp_printf("couldn't get packet for ping\n");
    }

    snprintf((char *) packet->data, csp_buffer_data_size(), "FTP ping");
    packet->length = (strlen((char *) packet->data) + 1); 
    if (!csp_send(conn, packet, 1000)) {
        ssp_printf("Send failed");	
        csp_buffer_free(packet);
    }

    csp_packet_t *p = csp_read(conn, 1000);
    if (p == NULL) {
        ssp_printf("ping failed\n");
    } else {
        
        memcpy(buff, (char *)p->data, p->length);
        ssp_printf("received: %s\n", buff);

        csp_close(conn);
        return 1;
    }

    csp_close(conn);
    return -1;
}

void csp_connection_server(uint8_t my_port, uint32_t packet_len, uint32_t time_out,
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *other), 
    int (*onTimeOut)(void *other),
    int (*checkExit)(void *other),
    void (*onExit)(void *other),
    void *other)
{

    int error = 0;
    //csp_socket_t *socket = csp_socket(CSP_SO_XTEAREQ | CSP_SO_HMACREQ | CSP_SO_CRC32REQ);
	//Create socket without any socket options //CSP_SO_NONE
    csp_socket_t *sock = csp_socket(CSP_SO_NONE);
    if (sock == NULL) {
        ssp_error("csp socket failed to initialize");
        return;
    }

	//Bind all ports to socket
	error = csp_bind(sock, my_port);
	if (error != CSP_ERR_NONE) {
	    ssp_error("csp socket failed to bind");
	    return;
	}
	//Create 10 connections backlog queue
	error = csp_listen(sock, 10);
    if (error != CSP_ERR_NONE) {
        ssp_error("csp socket failed to listen");
        return;
    }
	//Pointer to current connection and packet
	csp_conn_t *conn = NULL;
	csp_packet_t *packet;

    char *buff = ssp_alloc(packet_len, sizeof(char));
    if (buff == NULL) {
        ssp_printf("exiting serv thread\n");
        return;
    }


    for (;;) {

        if (get_exit() || checkExit(other))
            break;
    
        conn = csp_accept(sock, time_out);
        if (conn == NULL) {
            onTimeOut(other);
            continue;
        }
        
        ssp_printf("accepted\n");

        while(1) {

            if (get_exit() || checkExit(other))
                break;


            while ((packet = csp_read(conn, time_out)) != NULL) {

                memset(buff, 0, packet_len);
                memcpy(buff, (char *)packet->data, packet->length);

                if (onRecv(-1, buff, packet_len, NULL, conn, sizeof(struct csp_conn_s), other) == -1)
                    ssp_printf("recv failed\n");

                csp_buffer_free(packet);
            }
            //if the request has timeout, go back to waiting for accept
            if (!onTimeOut(other))
                break;

        }

        csp_close(conn);

    }
    ssp_free(buff);
    onExit(other);
}


void csp_connection_client(uint8_t dest_id, uint8_t dest_port, uint8_t my_port, uint32_t packet_len, uint32_t time_out, void*lock,
    int (*onSend)(int sfd, void *addr, size_t size_of_addr, void *onSendParams),
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *onRecvParams) ,
    int (*checkExit)(void *checkExitParams),
    void (*onExit)(void *params),
    void *params)
{

	csp_packet_t * packet = NULL;
	csp_conn_t * conn = NULL;

    char *buff = ssp_alloc(packet_len, sizeof(char));
    if (buff == NULL) {
        ssp_printf("exiting client thread\n");
        return;
    }

    while (1) {
        //lock will block, need to unlock when new request/s are started
        ssp_lock_take(lock);

        if (get_exit() || checkExit(params)){
            ssp_printf("exiting client thread\n");
            break;
        }
        
        /* Connect to host HOST, port PORT with regular UDP-like protocol and 1000 ms timeout */
        conn = csp_connect(CSP_PRIO_NORM, dest_id, dest_port, 100, CSP_SO_NONE);
        if (conn == NULL) {        
            continue;
        }
        
        onSend(-1, conn, sizeof(conn), params);

        while ((packet = csp_read(conn, time_out)) != NULL) {
            
            memcpy(buff, (char *)packet->data, packet_len);

            if (onRecv(-1, buff, packet_len, NULL, conn, sizeof(struct csp_conn_s), params) == -1)
                ssp_printf("recv failed\n");

            csp_buffer_free(packet);

        }       
        
        ssp_printf("closing connection\n");
        csp_close(conn);
        
        if (lock == NULL)
            break;

    }
    /* Close connection */
    if (conn != NULL)
        csp_close(conn);
    
    ssp_free(buff);
    onExit(params);
}
