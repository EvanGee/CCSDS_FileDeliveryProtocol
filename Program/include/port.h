/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef PORT_H
#define PORT_H

#define STACK_ALLOCATION 16384

//#define FREE_RTOS_PORT
#define POSIX_PORT

//#define RED_FS
#define POSIX_FILESYSTEM

#define CSP_NETWORK


//comment this out if you want to sendto function to actually work
//#define TEST

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
    #include <sys/stat.h>
    #include <dirent.h>
    #include <unistd.h>
    
    #define SSP_O_RDWR O_RDWR
    #define SSP_O_CREAT O_CREAT
    #define SSP_O_TRUNC O_TRUNC
    #define SSP_SEEK_SET SEEK_SET
    #define ssp_open open
    #define ssp_rename rename
    #define ssp_close close
    #define ssp_read read
    #define ssp_write write
    #define ssp_closedir closedir
    #define ssp_lseek lseek
    #define ssp_remove remove

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
    #include <csp/csp_endian.h>
    #define SSP_INET_ADDRSTRLEN 16
    #define SSP_AF_INET 2
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

#ifdef RED_FS

    #include <redposix.h>
    #define SSP_O_RDWR RED_O_RDWR
    #define SSP_O_CREAT RED_O_CREAT
    #define SSP_O_TRUNC RED_O_TRUNC
    #define SSP_SEEK_SET RED_SEEK_SET
    #define ssp_open red_open
    #define ssp_rename red_rename
    #define ssp_close red_close
    #define ssp_read red_read
    #define ssp_write red_write
    #define ssp_closedir red_closedir
    #define ssp_lseek red_lseek
    #define ssp_remove red_remove
    

#endif

//don't change these in the header file here, if you need to change them
//change them in the .c file
void ssp_error(char *msg);
void ssp_printf(char *stuff, ...);
void *ssp_alloc(uint32_t u_memb, size_t size);
void ssp_sendto(Response res);
void *ssp_thread_create(int stack_size, void * (thread_func)(void *params), void *params);
int ssp_time_count(void);
void ssp_error(char *error);
void ssp_free(void *pointer);
void ssp_thread_join(void *thread_handle);
int ssp_mkdir(char *dir_name);
void *ssp_opendir(char *dir_name);
int ssp_readdir(void *dir, char *file);
int get_exit();
void set_exit();

void reset_request(Request *req);


#endif
