#include "port.h"
#ifdef POSIX_PORT

/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "posix_server_provider.h"

#include <sys/select.h>
 
static int ssp_recvfrom(int sfd, void *buff, size_t packet_len, int flags, void *server_addr, uint32_t *server_addr_len) {
    int count = 0;
    count = recvfrom(sfd, buff, packet_len, flags, (struct sockaddr*)server_addr, (socklen_t*)server_addr_len);
    return count;
}

static void *ssp_init_socket_set(size_t *size) {
    fd_set *socket_set = ssp_alloc(1, sizeof(fd_set));
    *size = sizeof(fd_set);
    return (void *)socket_set;
}


static void ssp_fd_zero(void *socket_set){
    FD_ZERO((fd_set*) socket_set);
}

static void ssp_fd_set(int sfd, void *socket_set) {
    FD_SET(sfd, (fd_set*) socket_set);
}

static int ssp_fd_is_set(int sfd, void *socket_set){
    int is_set = 0;
    is_set = FD_ISSET(sfd, (fd_set*) socket_set);
    return is_set;
}

static void ssp_fd_clr(int sfd, void *socket_set) {
    FD_CLR(sfd, (fd_set *) socket_set);
}

static int ssp_select(int sfd, void *read_socket_set, void *write_socket_set, void *restrict_socket_set, uint32_t timeout_in_usec) {

    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = timeout_in_usec
    };

    int nrdy = select(sfd + 1, (fd_set *) read_socket_set, (fd_set *) write_socket_set, (fd_set *) restrict_socket_set, &timeout);

    return nrdy;
}

static void *ssp_init_sockaddr_struct(size_t *size_of_addr) {

        *size_of_addr = sizeof(struct sockaddr_storage);
        void *addr = ssp_alloc(1, sizeof(struct sockaddr_storage));
        if (addr == NULL)
            return NULL;

    return addr;
}


//if conn_typ == 1, tcp/ bind_to_host == 1 for binding local, 2 for connect
static int prepareHost(char *host_name, void *addr, size_t *size_of_addr, char *port, int conn_type, int bind_to_host)
{

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_V4MAPPED;
    hints.ai_socktype = conn_type;
    
    
    int err = getaddrinfo(host_name, port, &hints, &res);

    if (err != 0) {
        ssp_error("get addr info");
        return -1;
    }

    int sfd;
    struct addrinfo *cur;

    for (cur = res; cur != NULL; cur = cur->ai_next)
    {

        sfd = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);

        if (sfd < 0) {
            ssp_error("socket");
        }

        int val = 1;
        err = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
        if (err == -1) {
            ssp_error("set sock opt");
            ssp_close(sfd);
            continue;
        }
        
        if (bind_to_host == 1) {
            err = bind(sfd, cur->ai_addr, cur->ai_addrlen);
            if (err == -1) {
                ssp_error("bind");
                ssp_close(sfd);
                continue;
            }
        } else {
            err = connect(sfd, cur->ai_addr, cur->ai_addrlen);
            if (err == -1) { 
                ssp_error("connection with the server failed...\n"); 
                ssp_close(sfd);
                continue;
            }
        }

        ssp_memcpy(addr, cur->ai_addr, cur->ai_addrlen);
        *size_of_addr = cur->ai_addrlen;

        freeaddrinfo(cur);
        break;
    }

    if (cur == NULL)
    {
        ssp_printf("failed connecting with server\n");
        freeaddrinfo(res);
        return -1;
    }

    return sfd;
}
/*------------------------------------------------------------------------------
    This function is the inturpt handler for sigaction, right now i just 
    handle sigint, so we can exit nicely.
------------------------------------------------------------------------------*/

static void interuptHandler(int signum)
{
    switch (signum)
    {
    case SIGINT:
        set_exit();
        break;
    }
}


//see header file
int prepareSignalHandler()
{
    struct sigaction actionData;
    sigemptyset(&actionData.sa_mask);
    actionData.sa_handler = interuptHandler;
    actionData.sa_flags = 0;

    if (sigaction(SIGINT, &actionData, NULL) == -1)
    {
        ssp_error("sigaction sigint failed\n");
        return -1;
    }
    return 1;

}

static int resizeBuff(char **buffer, uint32_t *newBufferSize, uint32_t *prev_buff_size) {

     if (*newBufferSize != *prev_buff_size) {
            *buffer = realloc(*buffer, *newBufferSize);
            if (buffer == NULL) {
                return 1;
            } 
            return 0;
    }
    return 1;
}
//see header file
void connection_server(char *host_name, char* port, int initial_buff_size, int connection_limit, 
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *other), 
    int (*onTimeOut)(void *other),
    int (*checkExit)(void *other),
    void (*onExit)(void *other),
    void *other)
{
    size_t size_of_addr = 0;
    void *addr = ssp_init_sockaddr_struct(&size_of_addr);

    int sfd = prepareHost(host_name, addr, &size_of_addr, port, SOCK_STREAM, 1);
    if (sfd < 0)
        set_exit();

    socklen_t addr_len = size_of_addr;
    int err = listen(sfd, connection_limit);

    if (err == -1)
        ssp_error("listen failed\n");

    size_t size_of_socket_struct = 0;

    void *socket_set = ssp_init_socket_set(&size_of_socket_struct);
    void *read_socket_set = ssp_init_socket_set(&size_of_socket_struct);

    ssp_fd_zero(socket_set);
    ssp_fd_set(sfd, socket_set);
    ssp_fd_set(STDIN_FILENO, socket_set);

    uint32_t *buff_size = ssp_alloc(1, sizeof(uint32_t));
    if (buff_size == NULL)
        set_exit();

    *buff_size = initial_buff_size;
    uint32_t prev_buff_size = *buff_size;

    char *buff = ssp_alloc(sizeof(char), *buff_size);
    if (buff_size == NULL)
        set_exit();
    
    for (;;)
    {
        
        if (get_exit() || checkExit(other)){
            ssp_printf("exiting server thread\n");
            break;
        }
    
        ssp_memcpy(read_socket_set, socket_set, size_of_socket_struct);
        int nrdy = ssp_select(connection_limit + 1, read_socket_set, NULL,  NULL, 100e3);

        if(!resizeBuff(&buff, buff_size, &prev_buff_size)){
            ssp_printf("packet too large, cannot resize buffer\n");
        }

        if (nrdy == -1) {
            ssp_error("select");
            continue;
        }
        //timeout
        if (nrdy == 0) {
            if (onTimeOut(other) == -1)
                ssp_printf("timeout failed\n");

            continue;
        }

        for(int i = 0; i < connection_limit + 1; i++) {

            if (ssp_fd_is_set(i, read_socket_set)) {

                if (i == sfd) {
                    int new_socket = accept(i, (struct sockaddr*) addr, &addr_len);
                    if (new_socket < 0)
                        ssp_error ("accept failed");
                    
                    ssp_fd_set(new_socket, socket_set);
                    break;
                }

                int count = 0;
                while (count < *buff_size) {
                    count += ssp_recvfrom(i, &buff[count], *buff_size - count, 0, NULL, NULL);    
                    if (count < 0) {
                        ssp_error("recv failed server");
                        break;
                    }
                    else if (count == 0) {
                        ssp_error("connection finished");
                        ssp_fd_clr(i, socket_set);
                        ssp_close(i);
                        break;
                    } 
                }              

                if (count <= 0)
                    continue;

                int bytes_parsed = onRecv(i, buff, count, buff_size, addr, size_of_addr, other);
                if (bytes_parsed == -1) {
                    ssp_printf("recv failed somewhere in parsing\n");
                }
                
            
            }
        }
    }
    ssp_free(addr);
    ssp_free(read_socket_set);
    ssp_free(socket_set);
    ssp_free(buff_size);
    ssp_free(buff);
    ssp_close(sfd);
    onExit(other);
}




//see header file
void connectionless_server(char *host_name, char* port, int initial_buff_size, 
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *other), 
    int (*onTimeOut)(void *other),
    int (*checkExit)(void *other),
    void (*onExit)(void *other),
    void *other)
{

    size_t size_of_addr = 0;
    void *addr = ssp_init_sockaddr_struct(&size_of_addr);

    int sfd = prepareHost(host_name, addr, &size_of_addr, port, SOCK_DGRAM, 1);
    if (sfd < 0)
        set_exit();

    socklen_t addr_len = size_of_addr;
    size_t size_of_socket_struct = 0;

    void *socket_set = ssp_init_socket_set(&size_of_socket_struct);
    void *read_socket_set = ssp_init_socket_set(&size_of_socket_struct);

    ssp_fd_zero(socket_set);
    ssp_fd_set(sfd, socket_set);
    ssp_fd_set(STDIN_FILENO, socket_set);

    uint32_t *buff_size = ssp_alloc(1, sizeof(uint32_t));
    if (buff_size == NULL)
        set_exit();

    *buff_size = initial_buff_size + 10;
    uint32_t prev_buff_size = *buff_size;

    char *buff = ssp_alloc(sizeof(char), *buff_size);
    if (buff == NULL)
        set_exit();

    for (;;) {

        if (get_exit() || checkExit(other)){
            ssp_printf("exiting server thread\n");
            break;
        }
    
        ssp_memcpy(read_socket_set, socket_set, size_of_socket_struct);
        int nrdy = ssp_select(sfd + 1, read_socket_set, NULL,  NULL, 100e3);

        if(!resizeBuff(&buff, buff_size, &prev_buff_size)){
            ssp_printf("packet too large, cannot resize buffer\n");
        }

        if (nrdy == -1) {
            ssp_error("select");
            continue;
        }
        
        if (nrdy == 0) {
            if (onTimeOut(other) == -1)
                ssp_printf("timeout failed\n");
            continue;
        }

        if (ssp_fd_is_set(sfd, read_socket_set)) {
            int count = ssp_recvfrom(sfd, buff, *buff_size, 0, addr, &addr_len);
           
            if (count == -1)
                continue;

            else if (count >= *buff_size) {   
                ssp_printf("packet too large\n");
            }
            else {
                if (onRecv(sfd, buff, count, buff_size, addr, size_of_addr, other) == -1)
                    ssp_printf("recv failed server\n");
            }
        }
    }
    ssp_free(addr);
    ssp_free(read_socket_set);
    ssp_free(socket_set);
    ssp_free(buff_size);
    ssp_free(buff);
    ssp_close(sfd);
    onExit(other);
    
}




//https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpclient.c
void connectionless_client(char *hostname, char*port, int packet_len, void *params,
    int (*onSend)(int sfd, void *addr, size_t size_of_addr, void *params),
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *params) ,
    int (*checkExit)(void *params),
    void (*onExit)(void *params))
{

    int sfd, count;

    size_t size_of_addr = 0;
    void *addr = ssp_init_sockaddr_struct(&size_of_addr);

    sfd = prepareHost(hostname, addr, &size_of_addr, port, SOCK_DGRAM, 0);
    if (sfd < 0)
        set_exit();

    uint32_t addr_len = size_of_addr;
    uint32_t *buff_size = ssp_alloc(1, sizeof(uint32_t));
    if (buff_size == NULL)
        set_exit();

    *buff_size = packet_len + 10;

    uint32_t prev_buff_size = *buff_size;

    char *buff = ssp_alloc(sizeof(char), prev_buff_size);
    if (buff == NULL)
        set_exit();


    for (;;) {
        if (get_exit() || checkExit(params))
             break;
        
        if(!resizeBuff(&buff, buff_size, &prev_buff_size)){
            ssp_error("packet too large, cannot resize buffer\n");
        }

        if (onSend(sfd, addr, size_of_addr, params)) 
            ssp_error("send failed\n");

        count = ssp_recvfrom(sfd, buff, packet_len, MSG_DONTWAIT, addr, &addr_len);
       
        if (count == -1)
            continue;

        else if (count >= *buff_size){   
            ssp_error("packet too large\n");
            continue;
        }
        else{
            if (onRecv(sfd, buff, count, buff_size, addr, size_of_addr, params) == -1)
                ssp_error("recv failed client\n");
        }
        
    }

    ssp_free(addr);
    ssp_free(buff_size);
    ssp_free(buff);
    ssp_close(sfd);
    onExit(params);
}


//https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpclient.c
void connection_client(char *hostname, char*port, int packet_len, void *params,
    int (*onSend)(int sfd, void *addr, size_t size_of_addr, void *params),
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *params) ,
    int (*checkExit)(void *params),
    void (*onExit)(void *params))
{

    int sfd, count;

    size_t size_of_addr = 0;
    void *addr = ssp_init_sockaddr_struct(&size_of_addr);

    sfd = prepareHost(hostname, addr, &size_of_addr, port, SOCK_STREAM, 0);
    if (sfd < 0)
        set_exit();

    uint32_t addr_len = size_of_addr;
    uint32_t *buff_size = ssp_alloc(1, sizeof(uint32_t));
    if (buff_size == NULL)
        set_exit();

    *buff_size = packet_len;
    uint32_t prev_buff_size = *buff_size;

    char *buff = ssp_alloc(prev_buff_size, sizeof(char));
    if (buff == NULL)
        set_exit();


    for (;;) {
        
        if (get_exit() || checkExit(params))
             break;
        
        if(!resizeBuff(&buff, buff_size, &prev_buff_size)){
            ssp_printf("packet too large, cannot resize buffer\n");
        }

        if (onSend(sfd, addr, size_of_addr, params)) 
            ssp_error("send failed here\n");

        count = ssp_recvfrom(sfd, buff, packet_len, MSG_DONTWAIT, NULL, &addr_len);
       
        if (count < 0)
            continue;

        if (onRecv(sfd, buff, count, buff_size, addr, size_of_addr, params) == -1) {
            ssp_error("recv failed client\n");
            set_exit();
        }
        
    }
    ssp_free(addr);
    ssp_free(buff_size);
    ssp_free(buff);
    ssp_close(sfd);
    onExit(params);
}

#endif
