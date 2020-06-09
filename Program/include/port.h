/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef PORT_H
#define PORT_H

#define STACK_ALLOCATION 16384

#define POSIX_PORT
#define POSIX_FILESYSTEM
//#define CSP_NETWORK

#include "types.h"

#ifdef CSP_NETWORK
    #include "csp.h"
    typedef struct csp_packet_wrapper {
        uint8_t dest_id;
        uint8_t dest_port;
        uint8_t src_id;
        uint8_t src_port;
        csp_packet_t *packet;
    } csp_packet_wrapper;
#endif

#ifdef POSIX_FILESYSTEM
    #include <fcntl.h>
    #define SSP_O_RDWR O_RDWR
    #define SSP_O_CREAT O_CREAT
    #define SSP_O_TRUNC O_TRUNC
    #define SSP_SEEK_SET SEEK_SET
#endif

#ifdef POSIX_PORT
    #include <arpa/inet.h>
    #define SSP_INET_ADDRSTRLEN INET_ADDRSTRLEN
    #define SSP_AF_INET AF_INET
    #define ssp_htonl htonl 
    #define ssp_ntohl ntohl
    #define ssp_htons htons
    #define ssp_stonl stonl
    #define ssp_inet_ntop inet_ntop

    #include <string.h>
    #define ssp_memcpy memcpy

    #include <stdio.h>
    #define ssp_snprintf snprintf
    
    #include "stdlib.h"
    #define ssp_atol atol
#endif

#ifdef FREE_RTOS_PORT 
    //TODO need the above POSIX_PORT definitions to work, if we are bigendian, then the
    //htonl etc are empty.
#endif


//don't change these in the header file here, if you need to change them
//change them in the .c file
void ssp_error(char *msg);
void ssp_printf(char *stuff, ...);
void *ssp_alloc(uint32_t u_memb, size_t size);
void ssp_sendto(Response res);

void *ssp_thread_create(int stack_size, void * (thread_func)(void *params), void *params);
int ssp_time_count(void);

int ssp_open(char *pathname, int flags);
int ssp_read(int fd, char* buff, size_t size);
int ssp_lseek(int fd, int offset, int whence);
void ssp_error(char *error);
int ssp_write(int fd, const void *buf, size_t count);
int ssp_close(int fd);
void ssp_free(void *pointer);
void ssp_thread_join(void *thread_handle);
int ssp_remove(char *pathname);
int ssp_rename(const char *old, const char *new);
void reset_request(Request *req);


#endif