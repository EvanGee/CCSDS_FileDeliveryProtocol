/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#include "protocol_handler.h"
#include "app_control.h"
#include "port.h"
#include "mib.h"
#include "filesystem_funcs.h"
#include "types.h"

#ifdef POSIX_PORT
#include "posix_server_provider.h"
#else
typedef int socklen_t;
#endif

#ifdef CSP_NETWORK
#include "csp_server_provider.h"
#endif

//for print_request_state
#include "requests.h"

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);

/*------------------------------------------------------------------------------
    
    Generic functions

------------------------------------------------------------------------------*/

/*
//usefull for cpu clocks
static int is_negative(int number) {
    int negative_mask = 0x80000000;
    int is_negative = number & negative_mask;
    return is_negative;
}
*/

void reset_timeout(int *prevtime) {
    *prevtime = ssp_time_count();
}

static int check_timeout(int *prevtime, uint32_t timeout) {

    int prev = *prevtime;
    int current_time = ssp_time_count();
    int time_out = prev + timeout;

    if (current_time >= time_out) {
        *prevtime = current_time;
        return 1;
    }
    //wrap around the overflow condition
    else if (current_time < prev) {
        *prevtime = current_time;
    }
    return 0; 
}

//sets request procedure as clean_up if ttl has passed
static void timeout(Request *req, uint32_t time_out_before_cancel) {

    bool is_timeout = check_timeout(&req->timeout_before_cancel, time_out_before_cancel);
    if (is_timeout) {
        if (req->local_entity.transaction_finished_indication){
            ssp_printf("ACKNOWLEDGED request successfully sent without issue transaction: %llu\n", req->transaction_sequence_number);
        } else if (req->transmission_mode == UN_ACKNOWLEDGED_MODE){
            ssp_printf("UN_ACKNOWLEDGED request successfully sent without issue transaction: %llu\n", req->transaction_sequence_number);
        }
        else { 
            ssp_printf("stopped early, timed out without finishing request, saving req to be reopened later: %llu\n", req->transaction_sequence_number);
            print_request_state(req);
            save_req_to_file(req);
        }
        req->procedure = clean_up;
    } 
    
    is_timeout = check_timeout(&req->timeout_before_journal, time_out_before_cancel/2);
    if (is_timeout) {
        int error = save_req_to_file(req);
        if (error < 0) {
            ssp_printf("couldn't journal file\n");
        }
    }
}

void remove_request_check(Node *node, void *request, void *args) {
    Request *req = (Request *) request;
    List *req_list = (List *) args;

    if (req->procedure == clean_up) {
        ssp_printf("removing request\n");
        Request *remove_this = req_list->removeNode(req_list, node);

        if (req->local_entity.transaction_finished_indication || req->transmission_mode == UN_ACKNOWLEDGED_MODE) {
            int error = delete_saved_request(req);
            if (error < 0) {
                //TODO check if file exists, errno should be present
                //ssp_error("couldn't delete finished request, the request may have finished before journaling it\n");
            } 
                
        }

        ssp_cleanup_req(remove_this);
    }
}

/*------------------------------------------------------------------------------
    
    client callbacks

------------------------------------------------------------------------------*/


static int on_recv_client_callback(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *other) {
    
    Client *client = (Client *) other;
    if (packet_len > client->app->packet_len) {
        ssp_printf("packet received is too big for app\n");
        return -1;
    }

    Response res;
    res.addr = addr;
    res.sfd = sfd;
    res.type_of_network = client->remote_entity.type_of_network;
    res.size_of_addr = size_of_addr;
    res.transmission_mode = client->remote_entity.default_transmission_mode;
    res.msg = client->buff;
    res.packet_len = packet_len;

    Request **request_container = &client->current_request;
    Pdu_header incoming_pdu_header;

    int packet_index = process_pdu_header(packet, false, &incoming_pdu_header, res, request_container, client->request_list, client->app);
    if (packet_index < 0) {
        ssp_printf("error parsing header\n");
        return -1;
    }


    Request *current_request = (*request_container);

    parse_packet_client(packet, packet_index, res, current_request, client);

    reset_timeout(&current_request->timeout_before_cancel);

    memset(packet, 0, packet_len);
    
    return 0;
    
}

struct user_request_check_params {
    Response res;
    Client *client;
};

static void user_request_check(Node *node, void *request, void *args) {
    Request *req = (Request *) request;
    struct user_request_check_params* params = (struct user_request_check_params *) args;
    
    memset(params->res.msg, 0, params->client->packet_len);

    user_request_handler(params->res, req, params->client);
    timeout(req, params->client->remote_entity.transaction_inactivity_limit);
    
    remove_request_check(node, request, params->client->request_list);
}

static int on_send_client_callback(int sfd, void *addr, size_t size_of_addr, void *other) {

    Response res;    
    Client *client = (Client *) other;

    if (client->request_list->count == 0 && client->lock == NULL){
        client->close = true;
        return 0;
    }

    res.sfd = sfd;
    res.packet_len = client->packet_len;
    res.addr = addr;
    res.size_of_addr = size_of_addr;
    res.type_of_network = client->remote_entity.type_of_network;
    res.transmission_mode = client->remote_entity.default_transmission_mode;
    res.msg = client->buff;
    
    struct user_request_check_params params = {
        res,
        client
    };
    client->request_list->iterate(client->request_list, user_request_check, &params);     
    return 0;
}

/*------------------------------------------------------------------------------
    
    Server callbacks

------------------------------------------------------------------------------*/

static void client_check_callback(Node *node, void *client, void *args) {
    Client *c = (Client *) client;
    List *list = (List *) args;
    if (c->close) {
        Client *remove_this = (Client *) list->removeNode(list, node);
        ssp_printf("removing client, from server \n");
        ssp_client_join(remove_this);
    }
}


static void timeout_check_callback_server(Node *node, void *request, void *args) {
    Request *req = (Request *) request;
    FTP *app = (FTP *) args;

    on_server_time_out(req->res, req); 
    timeout(req, app->remote_entity.transaction_inactivity_limit);
    remove_request_check(node, request, app->request_list);
}

//return 1 if there are active requests, 0 if not
static int on_time_out_callback_server(void *other) {

    FTP *app = (FTP*) other;

    if (app->active_clients->count) {
        app->active_clients->iterate(app->active_clients, client_check_callback, app->active_clients);
    } 
    if (app->request_list->count) {
        app->request_list->iterate(app->request_list, timeout_check_callback_server, app);

    } else {
        return 0;
    }

    return 1;
}

static int on_recv_server_callback(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *other) {

    FTP *app = (FTP *) other;
    if (packet_len > app->packet_len) {
        ssp_printf("packet received is too big for app\n");
        return -1;
    }

    Response res;
    res.addr = addr;
    res.sfd = sfd;
    res.size_of_addr = size_of_addr;
    
    Pdu_header incoming_pdu_header;
    Request **request_container = &app->current_request;

    int packet_index = process_pdu_header(packet, true, &incoming_pdu_header, res, request_container, app->request_list, app);
    if (packet_index < 0) {
        ssp_printf("error parsing header\n");
        return -1;
    }

    Request *current_request = (*request_container);
    app->current_request = current_request;

    int count = parse_packet_server(packet, packet_index, app->current_request->res, current_request, incoming_pdu_header, app);

    reset_timeout(&current_request->timeout_before_cancel);

    memset(packet, 0, count);

    return count;

}
/*------------------------------------------------------------------------------
    
    check exit callbacks, will exit the app if returns 1

------------------------------------------------------------------------------*/

static int check_exit_server_callback(void *params) {
    FTP *app = (FTP*) params;
    if (app->close)
        return 1;
    return 0;
}

static int check_exit_client_callback(void *params) {
    Client *client = (Client*) params;
    if (client->close) 
        return 1;
    return 0;
}

/*------------------------------------------------------------------------------
    
    on Exit callbacks, used for cleaning up memory when exiting app

------------------------------------------------------------------------------*/

static void on_exit_client_callback (void *params) {
    Client *client = (Client *) params;
    if (client == NULL)
        return;

    client->close = true;
}

static void on_exit_server_callback (void *params) {
    FTP *app = (FTP*) params;
    ssp_cleanup_ftp(app);
}


/*------------------------------------------------------------------------------
    
    different server/client drivers

------------------------------------------------------------------------------*/

#ifdef POSIX_PORT
static
#endif
int get_ip_port(Remote_entity remote_entity, char *host_name, char *port){
    //convert int to char *
    int error = ssp_snprintf(port, 10, "%d", remote_entity.UT_port);
    if (error < 0) {
        ssp_error("ssp_snprintf");
        return -1;
    }

    uint32_t ut_addr = ssp_htonl(remote_entity.UT_address);

    //convert uint id to char *
    const char *ret = ssp_inet_ntop(SSP_AF_INET, &ut_addr, host_name, SSP_INET_ADDRSTRLEN);
    if (ret == NULL) {
        ssp_error("inet_ntop");
        return -1;
    }
    return 0;
}

void *ssp_connectionless_server_task(void *params) {
    #ifdef POSIX_PORT
        ssp_printf("starting posix connectionless server task\n");
        FTP* app = (FTP*) params;
        app->transaction_sequence_number = 1;

        char port[10];
        char host_name[SSP_INET_ADDRSTRLEN];

        int error = get_ip_port(app->remote_entity, host_name, port);
        if (error < 0) {
            ssp_cleanup_ftp(app);
            return NULL;
        }

        connectionless_server(host_name, port, 
            app->packet_len, 
            on_recv_server_callback, 
            on_time_out_callback_server, 
            check_exit_server_callback, 
            on_exit_server_callback, 
            app);
    #endif
    #ifndef POSIX_PORT
        ssp_printf("can't start posix connectionless server, no drivers\n");
    #endif  
    return NULL;
}

    
void *ssp_connectionless_client_task(void* params){
    #ifdef POSIX_PORT
        ssp_printf("starting posix connectionless client task \n");
        Client *client = (Client *) params;

        char port[10];
        char host_name[SSP_INET_ADDRSTRLEN];

        int error = get_ip_port(client->remote_entity, host_name, port);
        if (error < 0) {
            ssp_cleanup_client(client);
            return NULL;
        }

        connectionless_client(host_name, 
            port, 
            client->packet_len, 
            client, 
            on_send_client_callback, 
            on_recv_client_callback, 
            check_exit_client_callback, 
            on_exit_client_callback);
    #endif
    #ifndef POSIX_PORT
        ssp_printf("can't start posix connectionless client, no drivers\n");
    #endif  
    return NULL;
}

void *ssp_connection_server_task(void *params) {
    #ifdef POSIX_PORT
        ssp_printf("starting posix connection server\n");
        FTP* app = (FTP*) params;
        app->transaction_sequence_number = 1;

        char port[10];
        char host_name[SSP_INET_ADDRSTRLEN];

        int error = get_ip_port(app->remote_entity, host_name, port);
        if (error < 0) {
            ssp_cleanup_ftp(app);
            return NULL;
        }

        //1024 is the connection max limit
        connection_server(host_name, 
            port, 
            app->packet_len,
            10, 
            on_recv_server_callback, 
            on_time_out_callback_server,
            check_exit_server_callback, 
            on_exit_server_callback, 
            app);
    #endif
    #ifndef POSIX_PORT
        ssp_printf("can't start posix connection server, no drivers\n");
    #endif  
    return NULL;
}

void *ssp_connection_client_task(void *params) {
    #ifdef POSIX_PORT
        ssp_printf("starting posix connection client\n");
        Client *client = (Client *) params;

        char port[10];
        char host_name[SSP_INET_ADDRSTRLEN];

        int error = get_ip_port(client->remote_entity, host_name, port);
        if (error < 0) {
            ssp_cleanup_client(client);
            return NULL;
        }

    
        connection_client(host_name, 
            port, 
            client->packet_len, 
            client,
            on_send_client_callback, 
            on_recv_client_callback, 
            check_exit_client_callback, 
            on_exit_client_callback);
    #endif
    #ifndef POSIX_PORT
        ssp_printf("can't start posix connection client, no drivers\n");
    #endif  
    return NULL;
}

void *ssp_csp_connectionless_server_task(void *params) {
    #ifdef CSP_NETWORK
        ssp_printf("starting csp connectionless server\n");
        FTP *app = (FTP *) params;


        csp_connectionless_server(
            app->remote_entity.UT_port,
            app->remote_entity.mtu,
            app->remote_entity.async_NAK_interval,
            on_recv_server_callback, 
            on_time_out_callback_server,
            check_exit_server_callback, 
            on_exit_server_callback, 
            app);
    #endif
    #ifndef CSP_NETWORK
        ssp_printf("can't start csp connectionless server, no drivers\n");
    #endif  
    return NULL;
}

void *ssp_csp_connectionless_client_task(void *params) {
    #ifdef CSP_NETWORK
        ssp_printf("starting csp connectionless client\n");
        Client *client = (Client *) params;
        
        csp_connectionless_client(client->remote_entity.UT_address, 
            client->remote_entity.UT_port,
            CSP_ANY,
            client->remote_entity.mtu,
            on_send_client_callback, 
            on_recv_client_callback, 
            check_exit_client_callback, 
            on_exit_client_callback, 
            client);
    #endif
    #ifndef CSP_NETWORK
        ssp_printf("can't start csp connectionless client, no drivers\n");
    #endif  
    return NULL;
}


void *ssp_csp_connection_server_task(void *params) {
    #ifdef CSP_NETWORK
    ssp_printf("starting csp connection server\n");
    FTP *app = (FTP *) params;

        csp_connection_server(app->remote_entity.UT_port,
            app->remote_entity.mtu,
            app->remote_entity.async_NAK_interval,
            on_recv_server_callback,
            on_time_out_callback_server,
            check_exit_server_callback,
            on_exit_server_callback,
            params);
    #endif
    #ifndef CSP_NETWORK
        ssp_printf("can't start csp connection server, no drivers\n");
    #endif  
    return NULL;
} 

void *ssp_csp_connection_client_task(void *params) {
    #ifdef CSP_NETWORK
    ssp_printf("starting csp connection client\n");

    Client *client = (Client *) params;

    #ifdef FREE_RTOS_PORT

    void *lock = ssp_lock_create();
    if (lock == NULL)
        return NULL;

    //start out with open lock
    ssp_lock_give(lock);
    
    client->lock = lock;
    #endif
    
    csp_connection_client(client->remote_entity.UT_address, 
        client->remote_entity.UT_port,
        CSP_ANY,
        client->remote_entity.mtu,
        client->remote_entity.total_round_trip_allowance,
        client->lock,
        on_send_client_callback,
        on_recv_client_callback,
        check_exit_client_callback,
        on_exit_client_callback,
        params);

    #endif
    #ifndef CSP_NETWORK
        ssp_printf("can't start csp connection client, no drivers\n");
    #endif  
    return NULL;
}


void *ssp_generic_client_task(void *params) {
    ssp_printf("starting generic server task\n");
    return NULL;
}

void *ssp_generic_server_task(void *params) {
    ssp_printf("starting generic server task\n");
    return NULL;
}
/*------------------------------------------------------------------------------
    
    free functions

------------------------------------------------------------------------------*/

void ssp_cleanup_client(Client *client) {
    if (client == NULL)
        return;

    client->request_list->free(client->request_list, ssp_cleanup_req);
    ssp_lock_give(client->lock);
    ssp_lock_destory(client->lock);
    ssp_free(client->buff);
    ssp_free(client);
}

void ssp_client_join(Client *client) {
    
    ssp_thread_join(client->client_handle);
    ssp_cleanup_client(client);
}

static void exit_client(Node *node, void *element, void *args) {
    if (element == NULL)
        return;

    Client *client = (Client *) element;
    client->close = true;
    //in case we are currently blocking in the client thread
    ssp_lock_give(client->lock);
}

void ssp_cleanup_ftp(FTP *app) {
    app->request_list->free(app->request_list, ssp_cleanup_req);
    app->active_clients->iterate(app->active_clients, exit_client, NULL);
    app->active_clients->iterate(app->active_clients, client_check_callback, app->active_clients);
    app->active_clients->freeOnlyList(app->active_clients);   
    ssp_free(app->buff);
}
