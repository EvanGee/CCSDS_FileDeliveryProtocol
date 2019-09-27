
#include "packet.h"
#include "server.h"
#include "port.h"
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "mib.h"
#include "filesystem_funcs.h"
#include <fcntl.h>
#include "types.h"


#ifdef POSIX_PORT
        #include <pthread.h>
        #include <sys/socket.h>
        #include <stdio.h>
        #include <errno.h>
        #include <limits.h>
        #include <stdarg.h>
        #include <sys/select.h>
        #include <netinet/in.h>
        #include <signal.h>
        #include <sys/wait.h>
        #include <arpa/inet.h>
        #include <libgen.h>
        #include <netdb.h> 
#endif




#ifdef POSIX_FILESYSTEM
    #include <stdio.h>
    #include <unistd.h>

#endif


#ifdef POSIX_NETWORK
    #include <unistd.h>
    #include <sys/select.h>

#endif

#ifdef FREE_RTOS_PORT 
    #include "FreeRTOS.h"
    #include "task.h"
    #include "portable.h"

    #ifdef FREE_RTOS_PLUS
        #include ""
    #endif

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
    
    if (res.type_of_network == posix) {
        struct sockaddr* addr = (struct sockaddr*) res.addr;
        int err = ssp_write(res.sfd, res.msg, res.packet_len);
        if (err < 0) {
            ssp_error("ERROR in ssp_sendto");
        }

        /*
        int n = sendto(res.sfd, res.msg, res.packet_len, 0, addr, res.size_of_addr);
        if (n < 0) 
            ssp_error("ERROR in ssp_sendto");
        */


    }

}

int ssp_recvfrom(int sfd, void *buff, size_t packet_len, int flags, void *server_addr, uint32_t *server_addr_len) {
    int count = 0;
    #ifdef POSIX_NETWORK
        count = recvfrom(sfd, buff, packet_len, flags, (struct sockaddr*)server_addr, (socklen_t*)server_addr_len);
    #endif

    return count;
}

void *ssp_init_socket_set(size_t *size) {

    #ifdef POSIX_NETWORK
        fd_set *socket_set = ssp_alloc(1, sizeof(fd_set));
        *size = sizeof(fd_set);
    #endif
    return (void *)socket_set;
}


void ssp_fd_zero(void *socket_set){

    #ifdef POSIX_NETWORK
        FD_ZERO((fd_set*) socket_set);
    #endif
}

void ssp_fd_set(int sfd, void *socket_set) {
    #ifdef POSIX_NETWORK
        FD_SET(sfd, (fd_set*) socket_set);
    #endif
}

int ssp_fd_is_set(int sfd, void *socket_set){
    int is_set = 0;
    #ifdef POSIX_NETWORK
        is_set = FD_ISSET(sfd, (fd_set*) socket_set);
        
    #endif
    return is_set;
}

void ssp_fd_clr(int sfd, void *socket_set) {

    #ifdef POSIX_NETWORK
        FD_CLR(sfd, (fd_set *) socket_set);

    #endif 
}

int ssp_select(int sfd, void *read_socket_set, void *write_socket_set, void *restrict_socket_set, uint32_t timeout_in_usec) {

    #ifdef POSIX_NETWORK

    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = timeout_in_usec
    };

    int nrdy = select(sfd + 1, (fd_set *) read_socket_set, (fd_set *) write_socket_set, (fd_set *) restrict_socket_set, &timeout);
    #endif

    return nrdy;
}

void *ssp_init_sockaddr_struct(size_t *size_of_addr) {

    #ifdef POSIX_NETWORK

        *size_of_addr = sizeof(struct sockaddr_storage);
        void *addr = ssp_alloc(1, sizeof(struct sockaddr_storage));
        checkAlloc(addr, 1);


    #endif
    return addr;
}



/*------------------------------------------------------------------------------
    Std lib functions, for custom memory allocation, and stdio
------------------------------------------------------------------------------*/

void *ssp_alloc(uint32_t n_memb, size_t size) {
    
    #ifdef POSIX_PORT
        return calloc(n_memb, size);
    #endif

    #ifdef FREE_RTOS_PORT
        return pvPortMalloc(n_memb * size);
    #endif
    
}

void ssp_free(void *pointer) {

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
}

void ssp_printf( char *stuff, ...) {
    #ifdef POSIX_PORT
        va_list args;
        va_start(args, stuff);
        vfprintf(stdout, stuff, args);
        va_end (args);
        fflush(stdout);
    #endif
}



/*------------------------------------------------------------------------------
    Threading and task functions
------------------------------------------------------------------------------*/
void *ssp_thread_create(int stack_size, void * (thread_func)(void *params), void *params) {


    #ifdef POSIX_PORT
    pthread_t *handler = ssp_alloc(1,  sizeof(pthread_t));
    checkAlloc(handler, 1);

    pthread_attr_t *attr = ssp_alloc(1, sizeof(pthread_attr_t)); 
    checkAlloc(attr, 1);

    int err = pthread_attr_init(attr);
    if (0 != err) 
        perror("pthread_init failed");


    err = pthread_attr_setstacksize(attr, stack_size);

    if (0 != err)
        perror("ERROR pthread_attr_setstacksize %d");

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