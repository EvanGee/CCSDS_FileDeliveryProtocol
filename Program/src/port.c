/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#include "port.h"


#ifdef POSIX_PORT
    #include <pthread.h>
    #include <errno.h>
    #include <limits.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include <arpa/inet.h>
#endif

#ifdef POSIX_FILESYSTEM
    #include <unistd.h>
    #include <fcntl.h>
#endif

#ifdef FREE_RTOS_PORT 
    #include "FreeRTOS.h"
    #include "task.h"
    #include "portable.h"
#endif

#ifdef CSP_NETWORK
    #include "csp.h"
#endif


/*------------------------------------------------------------------------------
    File system port functions, these are used to interchange different 
    File systems
------------------------------------------------------------------------------*/
int ssp_rename(const char *old, const char *new) {
    #ifdef POSIX_FILESYSTEM
        return rename(old, new);
    #endif
}

int ssp_write(int fd, const void *buf, size_t count) {
    #ifdef POSIX_FILESYSTEM
        return write(fd, buf, count);
    #endif
}


int ssp_read(int fd, char* buff, size_t size) {
    #ifdef POSIX_FILESYSTEM
        return read(fd, buff, size);
    #endif

}

//SEEK_END 2  SEEK_CUR 1  SEEK_SET 0 
int ssp_lseek(int fd, int offset, int whence) {
    #ifdef POSIX_FILESYSTEM
        return lseek(fd, offset, whence);
    #endif
} 

int ssp_open(char *pathname, int flags) {
    #ifdef POSIX_FILESYSTEM
        //open with read and write permissions
        return open(pathname, flags, 0666);
    #endif
}

int ssp_close(int fd) {
    #ifdef POSIX_FILESYSTEM
        return close(fd);
    #endif
}


/*------------------------------------------------------------------------------
    Network port functions, these are used to interchange different network
    stacks
------------------------------------------------------------------------------*/

void ssp_sendto(Response res) {
    
    if (res.type_of_network == csp /*&& res.transmission_mode == UN_ACKNOWLEDGED_MODE*/) {
        #ifdef CSP_NETWORK
            csp_packet_t *packet = (csp_packet_t *) res.addr;
            csp_packet_t *packet_sending;

            if (csp_buffer_remaining() != 0) {
                packet_sending = csp_buffer_get(1);
                
                memcpy(packet_sending->data, res.msg, res.packet_len);
                int err = csp_sendto(0, packet->id.dst, packet->id.dport, packet->id.sport, 0, packet_sending, 10);
                
                if (err < 0) {
                    ssp_error("ERROR in ssp_sento");
                    csp_buffer_free(packet_sending);
                }
            }
            else 
                ssp_error("couldn't get new packet for sending!\n");
        #endif
    } else {
        #ifdef POSIX_PORT
            struct sockaddr* addr = (struct sockaddr*) res.addr;

            int err = sendto(res.sfd, res.msg, res.packet_len, 0, addr, sizeof(struct sockaddr));
            if (err < 0) {
                ssp_printf("res.sfd %d, res.packet_len %d, addr %d, addr size %d\n", res.sfd, res.packet_len, *addr, sizeof(struct sockaddr));
                ssp_error("ERROR in sendto");
            }
        #endif
    }
       
}



/*------------------------------------------------------------------------------
    Std lib functions, for custom memory allocation, and stdio
------------------------------------------------------------------------------*/

void *ssp_alloc(uint32_t n_memb, size_t size) {
    
    #ifdef POSIX_PORT
        void *mem = calloc(n_memb, size);
        if (mem == NULL)
            ssp_error("Memory failed to alloc!\n");
        return mem;
    #endif

    #ifdef FREE_RTOS_PORT
        return pvPortMalloc(n_memb * size);
    #endif
    
}

void ssp_free(void *pointer) {

    if (pointer == NULL)
        return;
        
    #ifdef POSIX_PORT
        free(pointer);
    #endif

    #ifdef FREE_RTOS_PORT
        vPortFree(pointer);
    #endif
    
}

void ssp_error(char *error){
    #ifdef POSIX_PORT
        perror(error);
    #endif
    #ifdef FREE_RTOS_PORT
        perror(error);
    #endif

}

//this can be switched to 
void ssp_printf(char *stuff, ...) {
    #ifdef POSIX_PORT
        va_list args;
        va_start(args, stuff);
        vfprintf(stdout, stuff, args);
        va_end (args);
        fflush(stdout);
    #endif

    #ifdef FREE_RTOS_PORT
        va_list args;
        va_start(args, stuff);
        vfprintf(stdout, stuff, args);
        va_end (args);
        fflush(stdout);
    #endif

}

//returns seconds elapsed
int ssp_time_count() {
    #ifdef POSIX_PORT
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);

        //clock_t c = clock();
        //c = c / CLOCKS_PER_SEC;
        return ts.tv_sec;
    #endif

    #ifdef FREE_RTOS_PORT
        //some kind of ticks
    #endif 
    return -1;
}

/*------------------------------------------------------------------------------
    Threading and task functions
------------------------------------------------------------------------------*/
void *ssp_thread_create(int stack_size, void * (thread_func)(void *params), void *params) {


    #ifdef POSIX_PORT
    pthread_t *handler = ssp_alloc(1,  sizeof(pthread_t));
    if (handler == NULL)
        return NULL;


    pthread_attr_t *attr = ssp_alloc(1, sizeof(pthread_attr_t)); 
    
    if (attr == NULL) {
        ssp_free(handler);
        return NULL;
    }

    int err = pthread_attr_init(attr);
    if (0 != err) 
        perror("pthread_init failed");


    err = pthread_attr_setstacksize(attr, stack_size);

    if (0 != err)
        ssp_error("ERROR pthread_attr_setstacksize %d");

    if (EINVAL == err) {
        printf("the stack size is less that PTHREAD_STACK_MIN %d\n", PTHREAD_STACK_MIN);
    }

    err = pthread_create(handler, attr, thread_func, params);
    if (0 != err)
        perror("ERROR pthread_create");

    ssp_free(attr);

    return handler;
    #endif

    #ifdef FREE_RTOS_PORT

    TaskHandle_t *xHandle = ssp_alloc(1, sizeof(TaskHandle_t));
    BaseType_t xReturned;
    
    /* Create the task, storing the handle. */
    xReturned = xTaskCreate(
                    thread_func,       /* Function that implements the task. */
                    "FTP",          /* Text name for the task. */
                    stack_size,      /* Stack size in words, not bytes. */
                    params,    /* Parameter passed into the task. */
                    tskIDLE_PRIORITY,/* Priority at which the task is created. */
                    xHandle );      /* Used to pass out the created task's handle. */
    if (xReturned == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
        ssp_error("Not enough memory to start task\n");

    return xHandle;
    #endif

}



//not required for Free_rtos
void ssp_thread_join(void *thread_handle) {
    #ifdef POSIX_PORT
        pthread_t * handle = (pthread_t*) thread_handle;
        pthread_join(*handle, NULL);
        ssp_free(thread_handle);
    #endif
}