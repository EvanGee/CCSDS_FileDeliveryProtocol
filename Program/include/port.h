/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef PORT_H
#define PORT_H


#define FREE_RTOS_PORT
#define RED_FS

#define CSP_NETWORK
//#define POSIX_FILESYSTEM
//#define POSIX_PORT

#include "types.h"
#define STACK_ALLOCATION 2000

#ifdef CSP_NETWORK
    #include "csp/csp.h"
    typedef struct csp_packet_wrapper {
        uint8_t dest_id;
        uint8_t dest_port;
        uint8_t src_id;
        uint8_t src_port;
        csp_packet_t *packet;
    } csp_packet_wrapper;

    #ifndef POSIX_PORT
        #include <csp/csp_endian.h>
        #define SSP_INET_ADDRSTRLEN 16
        #define SSP_AF_INET 2
        #define ssp_htonl csp_hton32
        #define ssp_ntohl csp_ntoh32
        #define ssp_ntohs csp_ntoh16
        #define ssp_htons csp_hton16
        #define ssp_inet_ntop inet_ntop
    #endif
#endif

#ifdef POSIX_FILESYSTEM
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <dirent.h>
    #include <unistd.h>
    #include <errno.h>
    
    #define SSP_O_RDWR O_RDWR
    #define SSP_O_CREAT O_CREAT
    #define SSP_O_TRUNC O_TRUNC
    #define SSP_SEEK_SET SEEK_SET
    #define SSP_SEEK_END SEEK_END
    #define SSP_EEXIST EEXIST
    #define ssp_open open
    #define ssp_rename rename
    #define ssp_close close
    #define ssp_read read
    #define ssp_write write
    #define ssp_closedir closedir
    #define ssp_lseek lseek
    #define ssp_remove remove
#endif

#ifdef RED_FS

    #include <redconf.h>
    #include <redfs.h>
    #include <redfse.h>
    #include <redposix.h>
    #include <redvolume.h>
    #include <redposix.h>
    #define SSP_O_RDWR RED_O_RDWR
    #define SSP_O_CREAT RED_O_CREAT
    #define SSP_O_TRUNC RED_O_TRUNC
    #define SSP_EEXIST RED_EEXIST
    #define SSP_SEEK_SET RED_SEEK_SET
    #define SSP_SEEK_END RED_SEEK_END
    #define ssp_open red_open
    #define ssp_rename red_rename
    #define ssp_close red_close
    #define ssp_read red_read
    #define ssp_write red_write
    #define ssp_closedir red_closedir
    #define ssp_lseek red_lseek
    #define ssp_remove red_unlink
    

#endif

#ifdef POSIX_PORT
    #include <arpa/inet.h>
    #include <pthread.h>
    #include <limits.h>

    #define SSP_INET_ADDRSTRLEN INET_ADDRSTRLEN
    
    #define SSP_AF_INET AF_INET
    #define ssp_htonl htonl 
    #define ssp_ntohl ntohl
    #define ssp_htons htons
    #define ssp_ntohs ntohs
    #define ssp_inet_ntop inet_ntop
    
    #include <string.h>
    #define ssp_memcpy memcpy

    #include <stdio.h>
    #define ssp_snprintf snprintf
    
    #include "stdlib.h"
    #define ssp_atol atol
    #define ssp_atoll atoll

    #define FMT64 "%lu"
#endif

#ifdef FREE_RTOS_PORT

    #include <string.h>
    #define ssp_memcpy memcpy

    #include <stdio.h>
    #define ssp_snprintf snprintf
    
    #include "stdlib.h"
    #define ssp_atol atol
    #define ssp_atoll atoll

    #define FMT64 "%llu"
#endif


//https://stackoverflow.com/questions/3022552/is-there-any-standard-htonl-like-function-for-64-bits-integers-in-c
#define ssp_htonll(x) ((1==ssp_htonl(1)) ? (x) : ((uint64_t)ssp_htonl((x) & 0xFFFFFFFF) << 32) | ssp_htonl((x) >> 32))
#define ssp_ntohll(x) ((1==ssp_ntohl(1)) ? (x) : ((uint64_t)ssp_ntohl((x) & 0xFFFFFFFF) << 32) | ssp_ntohl((x) >> 32))

//don't change these in the header file here, if you need to change them
//change them in the .c file
void ssp_error(char *msg);
void ssp_printf(char *stuff, ...);
void *ssp_alloc(uint32_t u_memb, size_t size);
void ssp_sendto(Response res);
void *ssp_thread_create(int stack_size, void* (thread_func)(void *params), void *params);
int ssp_time_count(void);
void ssp_error(char *error);
void ssp_free(void *pointer);
void ssp_thread_join(void *thread_handle);
int ssp_mkdir(char *dir_name);
void *ssp_opendir(char *dir_name);
int ssp_readdir(void *dir, char *file);
int get_exit(void);
void set_exit(void);
int ssp_lock_give(void *lock);
int ssp_lock_take(void *lock);
void *ssp_lock_create(void);
int ssp_lock_destory(void *lock);

void reset_request(Request *req);


#endif
