
#include "unit_tests.h"
#include <stdio.h>
#include "posix_server_provider.h"
#include "port.h"
#include "app_control.h"


//--------------------------------
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include "csp.h"

#define BUF_SIZE 500
static int clin(char *host, char*port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s, j;
    size_t len;
    ssize_t nread;
    char buf[BUF_SIZE];


    /* Obtain address(es) matching host/port */

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    s = getaddrinfo(host, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
        Try each address until we successfully connect(2).
        If socket(2) (or connect(2)) fails, we (close the socket
        and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                        rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

        close(sfd);
    }

    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);           /* No longer needed */

    /* Send remaining command-line arguments as separate
        datagrams, and read responses from server */

        if (write(sfd,  "stuff", 5) != 5) {
            fprintf(stderr, "partial/failed write\n");
            exit(EXIT_FAILURE);
        }

        nread = read(sfd, buf, BUF_SIZE);
        if (nread == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        return 0;
}

static int serv (char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
    ssize_t nread;
    char buf[BUF_SIZE];

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    s = getaddrinfo(NULL, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
        Try each address until we successfully bind(2).
        If socket(2) (or bind(2)) fails, we (close the socket
        and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                rp->ai_protocol);
        if (sfd == -1)
            continue;
        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                  /* Success */

        close(sfd);
    }

    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);           /* No longer needed */

    /* Read datagrams and echo them back to sender */

    for (;;) {
        peer_addr_len = sizeof(struct sockaddr_storage);
        nread = recvfrom(sfd, buf, BUF_SIZE, 0,
                (struct sockaddr *) &peer_addr, &peer_addr_len);
        if (nread == -1)
            continue;               /* Ignore failed request */

        char host[NI_MAXHOST], service[NI_MAXSERV];

        s = getnameinfo((struct sockaddr *) &peer_addr,
                        peer_addr_len, host, NI_MAXHOST,
                        service, NI_MAXSERV, NI_NUMERICSERV);
        if (s == 0)
            printf("Received %zd bytes from %s:%s\n",
                    nread, host, service);
        else
            fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));

        if (sendto(sfd, buf, nread, 0,
                    (struct sockaddr *) &peer_addr,
                    peer_addr_len) != nread)
            fprintf(stderr, "Error sending response\n");
    }
}



static int servCon (char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
    ssize_t nread;
    char buf[BUF_SIZE];

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    s = getaddrinfo(NULL, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
        Try each address until we successfully bind(2).
        If socket(2) (or bind(2)) fails, we (close the socket
        and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                rp->ai_protocol);
        if (sfd == -1)
            continue;
        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                  /* Success */

        close(sfd);
    }

    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);           /* No longer needed */

    int err = listen(sfd, 10);
    if (err < 0) {
        printf("error\n listen");
        return;
    }

    /* Read datagrams and echo them back to sender */

    for (;;) {
        peer_addr_len = sizeof(struct sockaddr_storage);

        printf("accepting\n");
        int cfd = accept(sfd, &peer_addr, peer_addr_len);

        printf("accepted\n");

        //nread = recvfrom(cfd, buf, BUF_SIZE, 0,
        //        (struct sockaddr *) &peer_addr, &peer_addr_len);

        nread = recv(cfd, buf, BUF_SIZE, 0);
        if (nread == -1) {
            printf("failed to read\n");
            continue;
        }

        

        char host[NI_MAXHOST], service[NI_MAXSERV];

        s = getnameinfo((struct sockaddr *) &peer_addr,
                        peer_addr_len, host, NI_MAXHOST,
                        service, NI_MAXSERV, NI_NUMERICSERV);
        if (s == 0)
            printf("Received %zd bytes from %s:%s\n",
                    nread, host, service);
        else
            fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));

        if (sendto(sfd, buf, nread, 0,
                    (struct sockaddr *) &peer_addr,
                    peer_addr_len) != nread)
            fprintf(stderr, "Error sending response\n");
    }
}




//--------------------------------------------------------------------

static int onRecvServer(int sfd, char *packet, uint32_t packet_len,  uint32_t *buff_size, void *addr, size_t size_of_addr, void *other){
    
    printf("received: %s\n", packet);

    
    Response res;
    res.addr = addr;
    res.msg = "hello back!!\n";
    res.packet_len = 10;
    res.size_of_addr = size_of_addr;
    res.sfd = sfd;
    res.type_of_network = csp;
    res.transmission_mode = ACKNOWLEDGED_MODE;
    
    ssp_sendto(res);
    

   return 0;
}

static int onTimeOut(void *other) {
    //printf("timeout\n");
    return 0;
}
static int onStdIn(void *other){
    return 0;
}
static int checkExit(void *other) {
    return 0;
}
static void onExit(void *other) {
}

//client stuff
static int onSend(int sfd, void *addr, size_t size_of_addr, void *onSendParams) {
    Response res;
    res.addr = addr;
    res.msg = "hello server!!\n";
    res.packet_len = 12;
    res.sfd = sfd;
    res.type_of_network = csp;
    res.transmission_mode = ACKNOWLEDGED_MODE;
    printf("sending!!!\n");
    
    ssp_sendto(res);

    return 0;
}
static int onRecvClient(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *onRecvParams) {
    printf("client received %s\n", packet);
    return 0;
}

static int checkExitClient(void *params) {
    return 0;
}

static void onExitClient(void *params) {

}

void *ssp_csp_connectionless_server_task_test(void *params) {
    printf("starting csp connectionless server\n");

    csp_connectionless_server(
    1, 
    onRecvServer, 
    onTimeOut, 
    onStdIn, 
    checkExit, 
    onExit, 
    params);

    return NULL;
}


void *ssp_csp_connectionless_client_task_test(void *params) {
    printf("starting csp connectionless client\n");
    csp_connectionless_client(1, 
    1, 
    2, 
    onSend, onRecvClient, checkExitClient, onExitClient, params);
    return NULL;
}


void *ssp_csp_connection_server_task_test(void *params) {
    csp_connection_server(1,
        onRecvServer,
        onTimeOut,
        onStdIn,
        checkExit,
        onExit,
        params);
}


void *ssp_csp_connection_client_task_test(void *params) {

    csp_connection_client(1, 1,
        onSend,
        onRecvClient,
        checkExitClient,
        onExitClient,
        params);
}
/*
void *ssp_csp_connectionless_client_task_test(void *params) {
    csp_connection_client(uint8_t dest_id, uint8_t dest_port, uint8_t src_port,
    int (*onSend)(int sfd, void *addr, uint32_t size_of_addr, void *onSendParams),
    int (*onRecv)(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *onRecvParams) ,
    int (*checkExit)(void *checkExitParams),
    void (*onExit)(void *params),
    void *params);
}
*/

int server_tests(int client){

    int buffsize = 10000;
    char buff[buffsize];
    
    /* Init buffer system with 10 packets of maximum 300 bytes each */
    printf("Initialising CSP\r\n");
    csp_buffer_init(100, 300);

	/* Init CSP with address MY_ADDRESS */
	csp_init(1);

	/* Start router task with 500 word stack, OS task priority 1 */
	csp_route_start_task(500, 1);


//    void *handle = ssp_thread_create(20000, ssp_csp_connectionless_server_task_test, NULL);
//    void *handle2 = ssp_thread_create(20000, ssp_csp_connectionless_client_task_test, NULL);
    void *handle = ssp_thread_create(20000, ssp_csp_connection_server_task_test, NULL);    
    void *handle2 = ssp_thread_create(20000, ssp_csp_connection_client_task_test, NULL);    
    //test_csp_connectionless_server();
    
    ssp_thread_join(handle);
    ssp_thread_join(handle2);


    if (client) {
        printf("I'm a client!\n");
        //connection_client("127.0.0.1", "1111", buffsize, NULL, NULL, NULL, NULL, onSend, onRecvClient, checkExitClient, onExitClient);
        //connectionless_client("localhost", "1111", buffsize, NULL, NULL, NULL, NULL, onSend, onRecvClient, checkExitClient, onExitClient);
        //csp_connectionless_client(1, 1, 2, 2, NULL, NULL, NULL, NULL, onSend, onRecvClient, checkExitClient, onExitClient);
        //csp_connection_client();
        //clin("127.0.0.1", "1111");
    }
    else {
        printf("I'm a server!\n");
        //connectionless_server("1111", buffsize, onRecvServer, onTimeOut, onStdIn, checkExit, onExit, NULL);
        
        //servCon ("1111");
        //csp_connectionless_server(1, 1, onRecvServer, onTimeOut, onStdIn, checkExit, onExit, NULL);
        //csp_connection_server();
    }
        
    return 0;
}
