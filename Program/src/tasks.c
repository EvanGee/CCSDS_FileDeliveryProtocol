


#include "protocol_handler.h"
#include "tasks.h"
#include "server.h"
#include "port.h"
#include <string.h>
#include "mib.h"
#include "filesystem_funcs.h"

#include "types.h"
#include "utils.h"
#include <arpa/inet.h>


//snprintf
#include <stdio.h>


//for print_request_state
#include "requests.h"
/*------------------------------------------------------------------------------
    
    Callbacks for the tasks bellow

------------------------------------------------------------------------------*/

//sets request procedure as clean_up if ttl passed
static void timeout(Request *req) {

    bool is_timeout = check_timeout(&req->timeout, TIMEOUT_BEFORE_CANCEL_REQUEST);
    if (is_timeout) {
        if (req->local_entity.transaction_finished_indication){
            ssp_printf("file successfully sent without issue transaction: %d\n", req->transaction_sequence_number);
        } else {
            ssp_printf("stopped early, timed out without finishing transaction, saving req to be reopened later: %d\n", req->transaction_sequence_number);
            print_request_state(req);
            save_req_to_file(req);
        }
        req->procedure = clean_up;
    }
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

    Request **request_container = &app->current_request;
    int packet_index = process_pdu_header(packet, true, res, request_container, app->request_list, app);
    if (packet_index < 0) {
        ssp_printf("error parsing header\n");
        return -1;
    }

    Request *current_request = (*request_container);
    app->current_request = current_request;
    
    parse_packet_server(packet, packet_index, app->current_request->res, current_request, app);

    reset_timeout(&current_request->timeout);

    memset(packet, 0, packet_len);
    return 0;

}


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
    
    Request **request_container = &client->current_request;

    int packet_index = process_pdu_header(packet, false, res, request_container, client->request_list, client->app);
    if (packet_index < 0) {
        ssp_printf("error parsing header\n");
        return -1;
    }

    Request *current_request = (*request_container);
    res.msg = current_request->buff;

    parse_packet_client(packet, packet_index, res, current_request, client);

    reset_timeout(&current_request->timeout);

    memset(packet, 0, packet_len);
    
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
    Request *req = (Request *) request;
    struct user_request_check_params* params = (struct user_request_check_params *) args;
    
    params->res.msg = req->buff;
    memset(params->res.msg, 0, params->client->packet_len);

    user_request_handler(params->res, req, params->client);
    timeout(req);
    
    remove_request_check(node, request, params->client->request_list);
}
//TODO can getrid of res here I think, well at least the addr
static int on_send_client_callback(int sfd, void *addr, size_t size_of_addr, void *other) {

    Response res;    
    Client *client = (Client *) other;

    if (client->request_list->count == 0){
        client->close = true;
        return 0;
    }

    res.sfd = sfd;
    res.packet_len = client->packet_len;
    res.addr = addr;
    res.size_of_addr = size_of_addr;
    res.type_of_network = client->remote_entity.type_of_network;
    res.transmission_mode = client->remote_entity.default_transmission_mode;

    struct user_request_check_params params = {
        res,
        client
    };

    client->request_list->iterate(client->request_list, user_request_check, &params);     
    return 0;
}



static void timeout_check_callback_server(Node *node, void *request, void *args) {
    Request *req = (Request *) request;
    on_server_time_out(req->res, req); 
    timeout(req);
    remove_request_check(node, request, args);
}


static void client_check_callback(Node *node, void *client, void *args) {
    Client *c = (Client *) client;
    List *list = (List *) args;
    if (c->close) {
        Client *remove_this = (Client *) list->removeNode(list, node);
        ssp_printf("removing client, from server \n");
        ssp_client_join(remove_this);
    }

}

//this function is a callback for servers
static int on_time_out_callback_server(void *other) {

    FTP *app = (FTP*) other;

    if (app->active_clients->count) {
        app->active_clients->iterate(app->active_clients, client_check_callback, app->active_clients);
    } 
    if (app->request_list->count) {
        app->request_list->iterate(app->request_list, timeout_check_callback_server, app->request_list);
    }

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



static int get_ip_port(Remote_entity remote_entity, char *host_name, char *port){
    //convert int to char *
    int error = snprintf(port, 10, "%d", remote_entity.UT_port);
    if (error < 0) {
        ssp_error("snprintf");
        return -1;
    }

    uint32_t ut_addr = htonl(remote_entity.UT_address);

    //convert uint id to char *
    const char *ret = inet_ntop(AF_INET, &ut_addr, host_name, INET_ADDRSTRLEN);
    if (ret == NULL) {
        ssp_error("inet_ntop");
        return -1;
    }
    return 0;
}
//------------------------------------------------------------------------------


void *ssp_connectionless_server_task(void *params) {
    ssp_printf("starting posix connectionless server task\n");
    FTP* app = (FTP*) params;
    app->transaction_sequence_number = 1;

    char port[10];
    char host_name[INET_ADDRSTRLEN];

    int error = get_ip_port(app->remote_entity, host_name, port);
    if (error < 0) {
        ssp_cleanup_ftp(app);
        return NULL;
    }

    connectionless_server(host_name, port, 
        app->packet_len, 
        on_recv_server_callback, 
        on_time_out_callback_server, 
        on_stdin_callback, 
        check_exit_server_callback, 
        on_exit_server_callback, 
        app);
    
    return NULL;
}

    
void *ssp_connectionless_client_task(void* params){
    ssp_printf("starting posix connectionless client task \n");
    Client *client = (Client *) params;

    char port[10];
    char host_name[INET_ADDRSTRLEN];

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
    
    return NULL;
}

void *ssp_connection_server_task(void *params) {
    ssp_printf("starting posix connection server\n");
    FTP* app = (FTP*) params;
    app->transaction_sequence_number = 1;

    char port[10];
    char host_name[INET_ADDRSTRLEN];

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
        on_stdin_callback, 
        check_exit_server_callback, 
        on_exit_server_callback, 
        app);

    return NULL;
}

void *ssp_connection_client_task(void *params) {
    ssp_printf("starting posix connection client\n");
    Client *client = (Client *) params;

    char port[10];
    char host_name[INET_ADDRSTRLEN];

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
  
    return NULL;
}

void *ssp_csp_connectionless_server_task(void *params) {
    ssp_printf("starting csp connectionless server\n");
    FTP *app = (FTP *) params;

    csp_connectionless_server(
        app->remote_entity.UT_port,
        on_recv_server_callback, 
        on_time_out_callback_server, 
        on_stdin_callback, 
        check_exit_server_callback, 
        on_exit_server_callback, 
        app);

    return NULL;
}

void *ssp_csp_connectionless_client_task(void *params) {
    ssp_printf("starting csp connectionless client\n");
    Client *client = (Client *) params;
    
    csp_connectionless_client(client->remote_entity.UT_address, 
        client->remote_entity.UT_port,
        client->app->remote_entity.UT_port, 
        on_send_client_callback, 
        on_recv_client_callback, 
        check_exit_client_callback, 
        on_exit_client_callback, 
        client);

    return NULL;
}


void *ssp_csp_connection_server_task(void *params) {
    ssp_printf("starting csp connection server\n");
    FTP *app = (FTP *) params;

    csp_connection_server(app->remote_entity.UT_port,
        on_recv_server_callback,
        on_time_out_callback_server,
        on_stdin_callback,
        check_exit_server_callback,
        on_exit_server_callback,
        params);

    return NULL;
} 

void *ssp_csp_connection_client_task(void *params) {
    ssp_printf("starting csp connection client\n");
    Client *client = (Client *) params;

    csp_connection_client(client->remote_entity.UT_address, 
        client->remote_entity.UT_port,
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

void ssp_cleanup_client(Client *client) {
    client->request_list->free(client->request_list, ssp_cleanup_req);
    ssp_free(client->buff);
    ssp_free(client);
}
void ssp_client_join(Client *client) {

    ssp_thread_join(client->client_handle);
    ssp_cleanup_client(client);
}

static void exit_client(Node *node, void *element, void *args) {
    Client *client = (Client *) element;
    client->close = true;
}

void ssp_cleanup_ftp(FTP *app) {
    app->request_list->free(app->request_list, ssp_cleanup_req);
    app->active_clients->iterate(app->active_clients, exit_client, NULL);
    app->active_clients->iterate(app->active_clients, client_check_callback, app->active_clients);
    app->active_clients->freeOnlyList(app->active_clients);
    ssp_free(app->buff);
    ssp_free(app);
}
