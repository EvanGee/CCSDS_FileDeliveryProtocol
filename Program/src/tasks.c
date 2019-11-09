


#include "protocol_handler.h"
#include "tasks.h"
#include "server.h"
#include "port.h"
#include <string.h>
#include "mib.h"
#include "filesystem_funcs.h"
#include <stdio.h>
#include "types.h"
#include <arpa/inet.h>
/*------------------------------------------------------------------------------
    
    Callbacks for the tasks bellow

------------------------------------------------------------------------------*/
static int on_recv_server_callback(int sfd, char *packet,  uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *other) {

    FTP *app = (FTP *) other;
    Response res;
    res.addr = addr;
    res.sfd = sfd;
    res.packet_len = app->packet_len;
    res.size_of_addr = size_of_addr;

    Request **request_container = &app->current_request;

    int packet_index = process_pdu_header(packet, 1, res, request_container, app->request_list, app);
    app->current_request = (*request_container);

    if (packet_index < 0)
        return -1;
    
    parse_packet_server(packet, packet_index, app->current_request->res, (*request_container), app);

    memset(packet, 0, res.packet_len);
    return 0;

}

static int on_recv_client_callback(int sfd, char *packet, uint32_t packet_len, uint32_t *buff_size, void *addr, size_t size_of_addr, void *other) {
    
    Client *client = (Client *) other;

    Response res;
    res.addr = addr;
    res.sfd = sfd;
    res.packet_len = client->packet_len;
    res.type_of_network = client->remote_entity->type_of_network;
    res.size_of_addr = size_of_addr;
    res.transmission_mode = client->remote_entity->default_transmission_mode;

    Request **request_container = &client->current_request;

    int packet_index = process_pdu_header(packet, 0, res, request_container, client->request_list, client->app);
    if (packet_index < 0) {
        ssp_printf("error parsing header\n");
        return -1;
    }
     
    res.msg = (*request_container)->buff;
    parse_packet_client(packet, packet_index, res, (*request_container), client);

    memset(packet, 0, res.packet_len);
    return 0;
    
}

void remove_request_check(Node *node, void *request, void *args) {
    Request *req = (Request *) request;
    List *req_list = (List *) args;
      
    if (req->procedure == clean_up) {
        ssp_printf("removing request\n");
        Request *remove_this = req_list->removeNode(req_list, node);
        ssp_cleanup_req(remove_this);
    }
}

struct user_request_check_params {
    Response res;
    Client *client;
};

static void user_request_check(Node *node, void *request, void *args) {
    Request * req = (Request *) request;
    struct user_request_check_params* params = (struct user_request_check_params *) args;
    
    params->res.msg = req->buff;
    memset(params->res.msg, 0, params->client->packet_len);

    user_request_handler(params->res, req, params->client);
    remove_request_check(node, request, params->client->request_list);
}

static int on_send_client_callback(int sfd, void *addr, size_t size_of_addr, void *other) {

    Response res;    
    Client *client = (Client *) other;
    if (client->request_list->count == 0){
        return 0;
    }
        
    res.sfd = sfd;
    res.packet_len = client->packet_len;
    res.addr = addr;
    res.size_of_addr = size_of_addr;
    res.type_of_network = client->remote_entity->type_of_network;
    res.transmission_mode = client->remote_entity->default_transmission_mode;

    struct user_request_check_params params = {
        res,
        client
    };

    client->request_list->iterate(client->request_list, user_request_check, &params);
    if (client->request_list->count == 0) {
        client->close = true;
    }
        
    return 0;
}



static void timeout_check_callback(Node *node, void *request, void *args) {
    Request *req = (Request *) request;
    on_server_time_out(req->res, req); 
    remove_request_check(node, request, args);
}


static void client_check_callback(Node *node, void *client, void *args) {
    Client *c = (Client *) client;
    List *list = (List *) args;

    if (c->close) {
        Client *remove_this = (Client *) list->removeNode(list, node);
        ssp_printf("removing client, from server \n");
        ssp_thread_join(c->client_handle);
        ssp_free(remove_this);
    }

}

//this function is a callback when using  my posix ports
static int on_time_out_callback(void *other) {

    FTP *app = (FTP*) other;
    if(app->current_request == NULL)
        return 0;

    app->request_list->iterate(app->request_list, timeout_check_callback, app->request_list);
    app->active_clients->iterate(app->active_clients, client_check_callback, app->active_clients);
    
    return 0;
}

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

static void on_exit_client_callback (void *params) {
    Client *client = (Client*) params;
    ssp_cleanup_client(client);
}

static void on_exit_server_callback (void *params) {
    FTP *app = (FTP*) params;
    ssp_cleanup_ftp(app);
}


//this function is just for posix fun
static int on_stdin_callback(void *other) {

    /*
    FTP *app = (FTP *) other;
    Request *req = app->newClient->req;

    char input[MAX_PATH];
    fgets(input, MAX_PATH, stdin);
    input[strlen(input)-1]='\0';
    
    if (req->procedure == none){
        if (strnlen(req->source_file_name, MAX_PATH) == 0){
            if (get_file_size(input) == -1){
                ssp_printf("file: %s, we had trouble opening this file, please enter a new file\n", input);
                return 0;
            }
            memcpy(app->newClient->req->source_file_name, input, MAX_PATH);
            ssp_printf("Enter a destination file name:\n");
        }
        else if (strnlen(req->destination_file_name, MAX_PATH) == 0){
            memcpy(app->newClient->req->destination_file_name, input, MAX_PATH);
            ssp_printf("sending file: %s As file named: %s To cfid enditity %d\n", app->newClient->req->source_file_name, app->newClient->req->destination_file_name, app->newClient->cfdp_id);
            ssp_printf("cancel connection mode (yes):\n");
        } 
        else if (strncmp(input, "yes", 3) == 0){
            ssp_printf("sending file connectionless\n");
            put_request(app->newClient->req->source_file_name, app->newClient->req->destination_file_name, 0, 0, 0, 1, NULL, NULL, app->newClient, app);
        } 
        else {
            ssp_printf("sending file connected\n");
            put_request(app->newClient->req->source_file_name, app->newClient->req->destination_file_name, 0, 0, 0, 0, NULL, NULL, app->newClient, app); 
        }
   }
   */
    return 0;

}


/*------------------------------------------------------------------------------
    
    Tasks

------------------------------------------------------------------------------*/
void *ssp_connectionless_server_task(void *params) {
    printf("starting posix connectionless server task\n");
    FTP* app = (FTP*) params;
    app->transaction_sequence_number = 1;

    char port[10];
    snprintf(port, 10, "%d",app->remote_entity->UT_port);
    
    connectionless_server(port, 
        app->packet_len, 
        on_recv_server_callback, 
        on_time_out_callback, 
        on_stdin_callback, 
        check_exit_server_callback, 
        on_exit_server_callback, 
        app);
    
    return NULL;
}

    
void *ssp_connectionless_client_task(void* params){
    printf("starting posix connectionless client task \n");
    Client *client = (Client *) params;

    char host_name[INET_ADDRSTRLEN];
    char port[10];

    //convert int to char *
    snprintf(port, 10, "%d", client->remote_entity->UT_port);

    //convert uint id to char *
    inet_ntop(AF_INET, &client->remote_entity->UT_address, host_name, INET_ADDRSTRLEN);
    
    connectionless_client(host_name, 
        port, 
        client->packet_len, 
        client, 
        client, 
        client, 
        client, 
        on_send_client_callback, 
        on_recv_client_callback, 
        check_exit_client_callback, 
        on_exit_client_callback);
    
    return NULL;
}

void *ssp_connection_server_task(void *params) {
    printf("starting posix connection server\n");
    FTP* app = (FTP*) params;
    app->transaction_sequence_number = 1;

    char port[10];
    snprintf(port, 10, "%u",app->remote_entity->UT_port);

    //1024 is the connection max limit
    connection_server(port, 
        app->packet_len,
        10, 
        on_recv_server_callback, 
        on_time_out_callback, 
        on_stdin_callback, 
        check_exit_server_callback, 
        on_exit_server_callback, 
        app);

    return NULL;
}

void *ssp_connection_client_task(void *params) {
    printf("starting posix connection client\n");
    Client *client = (Client *) params;

    char host_name[INET_ADDRSTRLEN];
    char port[10];

    //convert int to char *
    snprintf(port, 10, "%d", client->remote_entity->UT_port);

    //convert uint id to char *
    inet_ntop(AF_INET, &client->remote_entity->UT_address, host_name, INET_ADDRSTRLEN);

    connection_client(host_name, 
        port, 
        client->packet_len, 
        client, 
        client, 
        client, 
        client, 
        on_send_client_callback, 
        on_recv_client_callback, 
        check_exit_client_callback, 
        on_exit_client_callback);
  
    return NULL;
}

void *ssp_csp_connectionless_server_task(void *params) {
    printf("starting csp connectionless server\n");
    FTP *app = (FTP *) params;

    csp_connectionless_server(
        app->remote_entity->UT_port,
        on_recv_server_callback, 
        on_time_out_callback, 
        on_stdin_callback, 
        check_exit_server_callback, 
        on_exit_server_callback, 
        app);

    return NULL;
}

void *ssp_csp_connectionless_client_task(void *params) {
    printf("starting csp connectionless client\n");
    Client *client = (Client *) params;
    
    csp_connectionless_client(client->remote_entity->UT_address, 
        client->remote_entity->UT_port,
        client->app->remote_entity->UT_port, 
        on_send_client_callback, 
        on_recv_client_callback, 
        check_exit_client_callback, 
        on_exit_client_callback, 
        client);

    return NULL;
}


void *ssp_csp_connection_server_task(void *params) {
    printf("starting csp connection server\n");
    FTP *app = (FTP *) params;

    csp_connection_server(app->remote_entity->UT_port,
        on_recv_server_callback,
        on_time_out_callback,
        on_stdin_callback,
        check_exit_server_callback,
        on_exit_server_callback,
        params);

    return NULL;
} 

void *ssp_csp_connection_client_task(void *params) {
    printf("starting csp connection client\n");
    Client *client = (Client *) params;

    csp_connection_client(client->remote_entity->UT_address, 
        client->remote_entity->UT_port,
        on_send_client_callback,
        on_recv_client_callback,
        check_exit_client_callback,
        on_exit_client_callback,
        params);

    return NULL;
}
/*------------------------------------------------------------------------------
    
    free functions

------------------------------------------------------------------------------*/

static void ssp_client_join(Node *node, void *element, void*args) {
    Client *client = (Client *) element;
    ssp_thread_join(client->client_handle);    
}

void ssp_join_clients(List *clients) {
    clients->iterate(clients, ssp_client_join, NULL);
}

static void exit_client(Node *node, void *element, void *args) {
    Client *client = (Client *) element;
    client->close = true;
}

void ssp_cleanup_ftp(FTP *app) {
    app->request_list->free(app->request_list, ssp_cleanup_req);
    app->active_clients->iterate(app->active_clients, exit_client, NULL);

    app->active_clients->iterate(app->active_clients, client_check_callback, app->active_clients);

    free_mib(app->mib);
    app->active_clients->freeOnlyList(app->active_clients);
    ssp_free(app);
}

void ssp_cleanup_client(Client *client) {
    client->request_list->free(client->request_list, ssp_cleanup_req);
    ssp_cleanup_pdu_header(client->pdu_header);
    
}
