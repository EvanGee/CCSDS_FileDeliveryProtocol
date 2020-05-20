#ifndef POSIX_SERVER_H
#define POSIX_SERVER_H


#include <netinet/in.h>
int *prepareSignalHandler(void);
/*------------------------------------------------------------------------------
    Purpose:    This function creates a udp select server on the socket sfd.
    Perameters: int sfd: The socket descriptor created by prepareUdpHost
                void *other: this is the void* that we want to pass into 
                onRecv, and onTimeout
    Return:     None
------------------------------------------------------------------------------*/
/*-----------------------------CALLBACK onRecv----------------------------------
    Purpose:    This function handles what happends when a user receives a 
                packet
    Perameters: int sfd: Is the socket descriptor that the msg was received on
                this is useful for resending messges.
                char *msg: Is the packet that was sent
                struct sockaddr_storage client: Is the address of the sender
                void *other: Is the void * we passed into selectUdp
    Return:     None
------------------------------------------------------------------------------*/
/*-----------------------------CALLBACK onTimeOut-------------------------------
    Purpose:    This function 
    Perameters: void *other: Is the void * we passed into selectUdp
    Return:     None
------------------------------------------------------------------------------*/

/*-----------------------------CALLBACK onTimeOut-------------------------------
    Purpose:    This is a simple udp client 
    Perameters: hostname is the name of an address eg, 127.0.0.1, port is the por
                channel_size is the size of the channel in bytes
    eg, 1111
    Return:     None
------------------------------------------------------------------------------*/


void connectionless_server(char *host_name, char* port, int initial_buff_size, 
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *other), 
    int (*onTimeOut)(void *other),
    int (*onStdIn)(void *other),
    int (*checkExit)(void *other),
    void (*onExit)(void *other),
    void *other);

void connection_server(char *host_name, char* port, int initial_buff_size, int connection_limit,
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *other), 
    int (*onTimeOut)(void *other),
    int (*onStdIn)(void *other),
    int (*checkExit)(void *other),
    void (*onExit)(void *other),
    void *other);

void connection_client(char *hostname, char*port, int packet_len, void *params,
    int (*onSend)(int sfd, void *addr, size_t size_of_addr, void *params),
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *params) ,
    int (*checkExit)(void *params),
    void (*onExit)(void *params));

void connectionless_client(char *hostname, char*port, int packet_len, void *params,
    int (*onSend)(int sfd, void *addr, size_t size_of_addr, void *params),
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *params) ,
    int (*checkExit)(void *params),
    void (*onExit)(void *params));

#endif