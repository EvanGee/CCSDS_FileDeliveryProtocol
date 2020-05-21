
#ifndef PORT_H
#define PORT_H

#define STACK_ALLOCATION 16384




#define POSIX_PORT
#define POSIX_FILESYSTEM
#define POSIX_NETWORK

#define CSP_NETWORK
//#define FREE_RTOS_PORT

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


void ssp_error( char *msg);
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

int ssp_rename(const char *old, const char *new);
void reset_request(Request *req);


#endif