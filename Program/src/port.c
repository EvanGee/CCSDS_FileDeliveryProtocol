/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#include "port.h"

static int exit_now = 0;

#ifdef POSIX_PORT
    #include <errno.h>
    #include <limits.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include <arpa/inet.h>
    #include <stdarg.h>
    #include <time.h>

#endif

#ifdef POSIX_FILESYSTEM
    #include <unistd.h>
    #include <fcntl.h>
    #include <dirent.h>
    #include "utils.h"
#endif

#ifdef RED_FS
    #include <redposix.h>
#endif

#ifdef FREE_RTOS_PORT 
    #include "FreeRTOS.h"
    #include <os_task.h>
    #include <os_queue.h>
    //#include "portable.h" //not sure what i need here for sat build

    //make sure these are available in FREERTOS
    #include <errno.h>
    #include <time.h>
    #include <limits.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include <stdarg.h>
    #include <semphr.h>

#endif

#ifdef CSP_NETWORK
    #include "csp/csp.h"
#endif


/*------------------------------------------------------------------------------
    File system port functions, these are used to interchange different 
    File systems, will add RELIANCE_EDGE here in the future
------------------------------------------------------------------------------*/

int get_exit() {
    return exit_now;
}

void set_exit() {
    exit_now = 1;
}


int ssp_mkdir(char *dir_name) {
    #ifdef POSIX_FILESYSTEM
        int error = mkdir(dir_name, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (error < 0) {
            if(errno == EEXIST) {
                return 1;
            } 
            //ssp_printf("couldn't make dir\n");
            return -1;   
        }     
        else {
            //ssp_printf("%s directory created\n", dir_name);
            return 1;
        }

    #endif
    #ifdef RED_FS

        int error = red_mkdir(dir_name);
        if (error < 0) {
            if(red_errno == RED_EEXIST) {
                return 1;
            } 
            ssp_printf("couldn't make dir\n");
            return -1;   
        }     
        else {
            ssp_printf("%s directory created\n", dir_name);
            return 1;
        }
    #endif
}

void *ssp_opendir(char *dir_name) {

    #ifdef POSIX_FILESYSTEM
        DIR *dir;
        dir = opendir(dir_name);
        if(dir == NULL){
            ssp_error("Unable to open directory");
            return NULL;
        }
        return dir;
    #endif
    #ifdef RED_FS

        REDDIR *dir;
        dir = red_opendir(dir_name);
        if(dir == NULL){
            ssp_error("Unable to open directory");
            return NULL;
        }
        return dir;

    #endif
}

int ssp_readdir(void *dir, char *file){
    #ifdef POSIX_FILESYSTEM
        struct dirent *file_read;
        DIR *d = (DIR*)dir;

        file_read=readdir(d);
        if (file_read == NULL) {
            return 0;
        }
        ssp_memcpy(file, file_read->d_name, MAX_PATH);

        return 1;
    #endif
    #ifdef RED_FS 
    
        REDDIRENT *file_read;
        REDDIR *d = (REDDIR*)dir;

        file_read=red_readdir(d);
        if (file_read == NULL) {
            return 0;
        }
        ssp_memcpy(file, file_read->d_name, MAX_PATH);

        return 1;
    #endif    
}

#include "packet.h"
/*------------------------------------------------------------------------------
    Network port functions, these are used to interchange different network
    stacks
------------------------------------------------------------------------------*/
#ifdef FREE_RTOS_PORT
extern QueueHandle_t sendQueue;
#endif

void ssp_sendto(Response res) {

    #ifdef CSP_NETWORK
        csp_packet_t *packet_sending;
        csp_packet_t *packet;
        csp_conn_t *conn;
    #endif
    int err = 0;

    switch (res.type_of_network)
    {
    case generic:
        /* code */
        #ifdef FREE_RTOS_PORT
        while (true) {
            if (xQueueSendToBack(sendQueue, res.msg, 100) != pdPASS)
                ssp_printf("queue full, failed to post packet, blocking task untill sent\n");
            else
                break;
        }
        return;
        #else
        ssp_printf("FreeRtos not defined, can't use generic queues");
        break;
        #endif
    case csp_connectionless:
        #ifdef CSP_NETWORK
        packet = (csp_packet_t *) res.addr;

        if (csp_buffer_remaining() != 0) {
            packet_sending = csp_buffer_get(1);
            if (packet_sending == NULL) {
                ssp_printf("couldn't get packet, is NULL");
            }  
                            
            //ssp_printf("sending packet to dest %d port %d srcaddr %d srcport %d \n", packet->id.dst, packet->id.dport, packet->id.src, packet->id.sport);
        
            ssp_memcpy(packet_sending->data, res.msg, res.packet_len);
            packet_sending->length = res.packet_len;

            err = csp_sendto(packet->id.pri, packet->id.dst, packet->id.dport, packet->id.sport, 0, packet_sending, 100);
            
            if (err < 0) {
                ssp_error("ERROR in ssp_sento");
                csp_buffer_free(packet_sending);
            }
        }
        else 
            ssp_error("couldn't get new packet for sending!\n");
        #else
        ssp_printf("CSP network not defined, but network is trying to use it\n");
        #endif
        break;

    case csp_connection:
        #ifdef CSP_NETWORK
            conn = (csp_conn_t*) res.addr;

            if (csp_buffer_remaining() != 0) {
                packet_sending = csp_buffer_get(1);
                if (packet_sending == NULL) {
                    ssp_printf("couldn't get packet, is NULL");
                }
                ssp_memcpy(packet_sending->data, res.msg, res.packet_len);
                packet_sending->length = res.packet_len;
                /* 5. Send packet */
                if (!csp_send(conn, packet_sending, 1000)) {
                    /* Send failed */
                    csp_log_error("Send failed");
                    csp_buffer_free(packet_sending);
                }
            }
        #else 
        ssp_printf("CSP network not defined, but network is trying to use it\n");
        #endif        
        break;

    case test:
        break;

    default:
        #ifdef POSIX_PORT
        
        //ssp_print_bits(res.msg, 10);
        //Pdu_header header;
        //memset(&header, 0, sizeof(Pdu_header));
        //get_pdu_header_from_packet(res.msg, &header);
        //ssp_print_header(&header);

        err = sendto(res.sfd, res.msg, res.packet_len, 0, (struct sockaddr*)res.addr, sizeof(struct sockaddr));
        if (err < 0) {
            ssp_printf("res.sfd %d, res.packet_len %d, addr %d, addr size %d\n", res.sfd, res.packet_len, (struct sockaddr*)res.addr, sizeof(struct sockaddr));
            ssp_error("ERROR in sendto");
        }
        break;
        #else
        ssp_printf("Posix network not defined, but network is trying to use it\n");
        #endif
        break;
    }
       
}



/*------------------------------------------------------------------------------
    Std lib functions, for custom memory allocation, and stdio
------------------------------------------------------------------------------*/

void *ssp_alloc(uint32_t n_memb, size_t size) {
    
    #ifdef FREE_RTOS_PORT
        void *mem = pvPortMalloc(n_memb * size);
        memset(mem, 0, n_memb * size);
    #else
        void *mem = calloc(n_memb, size);

        if (mem == NULL)
            ssp_error("Memory failed to alloc!\n");
    #endif
    return mem;
}

void ssp_free(void *pointer) {

    if (pointer == NULL)
        return;
    
    #ifdef FREE_RTOS_PORT
        vPortFree(pointer);
    #else
        free(pointer);
    #endif
    
}

//what kind of errorno functions do we have in RED_FS?
void ssp_error(char *error){
    ssp_printf("ERROR: ");
    perror(error);
}

//this can be switched to printing to a log file in the future, not sure
//if FREE_RTOS has va_list
void ssp_printf(char *stuff, ...) {
    va_list args;
    va_start(args, stuff);
    #ifdef FREE_RTOS_PORT
        vfprintf(stdout, stuff, args);
        va_end (args);
        fflush(stdout);
    #else
    
        char log_string[2000];
        vsnprintf(log_string, sizeof(log_string), stuff, args);
        log_ftp("INFO", log_string);
        va_end (args);

    #endif
    
}

#ifdef FREE_RTOS_PORT
    //replace with real time clock
    static int shit_time = 1;
#endif
//returns seconds elapsed, need FREE RTOS realtime clock lib to properly port
int ssp_time_count() {

    #ifdef FREE_RTOS_PORT
        return shit_time++;//INSERT TIME FUNCTIONS get delta time since last call
    #else
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return ts.tv_sec;
    #endif
}


/*------------------------------------------------------------------------------
    Threading and task functions
------------------------------------------------------------------------------*/
void *ssp_thread_create(int stack_size, void* (thread_func)(void *params), void *params) {

    #ifdef FREE_RTOS_PORT

    TaskHandle_t *xHandle = ssp_alloc(1, sizeof(TaskHandle_t));
    BaseType_t xReturned;
    
    // Create the task, storing the handle.
    xReturned = xTaskCreate(
                    (TaskFunction_t) thread_func, // Function that implements the task.
                    "FTP",          // Text name for the task.
                    stack_size,      // Stack size in words, not bytes.
                    params,    // Parameter passed into the task.
                    1,// Priority at which the task is created.
                    xHandle );      // Used to pass out the created task's handle.

    if (xReturned == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) {
        ssp_error("Not enough memory to start task\n");
        return NULL;
    }

    return xHandle;

    #else //pthreads
    pthread_t *handler = ssp_alloc(1,  sizeof(pthread_t));
    if (handler == NULL) 
        return NULL;

    pthread_attr_t *attr = ssp_alloc(1, sizeof(pthread_attr_t)); 
    if (attr == NULL) {
        ssp_free(handler);
        return NULL;
    }

    int err = pthread_attr_init(attr);
    if (0 != err) {
        ssp_error("pthread_init failed");
        ssp_free(attr);
        ssp_free(handler);
    }
    err = pthread_attr_setstacksize(attr, stack_size);
    if (0 != err)
        ssp_error("pthread_attr_setstacksize %d");
        
    if (EINVAL == err) {
        ssp_printf("the stack size is less that PTHREAD_STACK_MIN %d\n", PTHREAD_STACK_MIN);
        ssp_free(attr);
        ssp_free(handler);
        return NULL;
    }

    err = pthread_create(handler, attr, thread_func, params);
    if (0 != err) {
        ssp_error("pthread_create");
        ssp_free(handler);
    }
    ssp_free(attr);

    return handler;
    #endif
}


void *ssp_lock_create() {
    #ifdef FREE_RTOS_PORT

    /* Create a mutex type semaphore. */
    void *lock =  xSemaphoreCreateBinary();
    if (lock == NULL) {
        ssp_printf("mutex init failed\n");
        return NULL;
    }

    #else
    pthread_mutex_t *lock = ssp_alloc(1, sizeof(pthread_mutex_t));
    if (lock == NULL) {
        return NULL;
    }

    if (pthread_mutex_init(lock, NULL) != 0) {
        ssp_printf("mutex init failed\n");
        ssp_free(lock);
        return NULL;
    }

    #endif
    return lock;
}

int ssp_lock_destory(void *lock) {
    if (lock == NULL) {
        return 0;
    }

    #ifdef FREE_RTOS_PORT
    #else
    int error = pthread_mutex_destroy(lock);

    if (!error) {
        ssp_free(lock);
        return 1;

    } else if (error == EBUSY){
        ssp_printf("mutex is currently locked\n");
    }
    #endif
    return 0;
}

int ssp_lock_give(void *lock) {
    if (lock == NULL) {
        return 0;
    }

    #ifdef FREE_RTOS_PORT
    SemaphoreHandle_t xSemaphore = (SemaphoreHandle_t) lock;
 
    if (xSemaphoreGive( xSemaphore ) == pdTRUE) {
        //ssp_printf("gave the lock\n");
        return 1;
    }
    ssp_printf("couldn't give the lock\n");

    #else
    pthread_mutex_t *mutex = (pthread_mutex_t *) lock; 
    
    int error = pthread_mutex_unlock(mutex);
    if (!error) {
        return 1;

    } else if (error == EBUSY){
        ssp_printf("mutex is currently locked\n");
    }



    #endif
    return 0;
}

int ssp_lock_take(void *lock) {
    if (lock == NULL) {
        return 0;
    }

    #ifdef FREE_RTOS_PORT
    SemaphoreHandle_t xSemaphore = (SemaphoreHandle_t) lock;
    if( xSemaphore == NULL ) {
        return 0;
    }

    //ssp_printf("waiting for client lock\n");
    if (xSemaphoreTake( xSemaphore, portMAX_DELAY) == pdTRUE) {
        //ssp_printf("took the lock\n");
        return 1;
    } else {
        ssp_printf("couldn't take the lock\n");
        return 0;
    }
    #else

    pthread_mutex_t *mutex = (pthread_mutex_t *) lock; 
    
    int error = pthread_mutex_lock(mutex);
    if (!error) {
        return 1;

    } else {
        ssp_printf("mutex lock take error %d\n", error);
    }

    return 0;
    #endif
}



//not required for Free_rtos
void ssp_thread_join(void *thread_handle) {
    #ifdef POSIX_PORT
        pthread_t * handle = (pthread_t*) thread_handle;
        pthread_join(*handle, NULL);
        ssp_free(thread_handle);
    #endif
    #ifdef FREE_RTOS_PORT
        ssp_printf("deleting client task\n");
        // Doesn't work vTaskDelete(NULL);
    #endif
}
