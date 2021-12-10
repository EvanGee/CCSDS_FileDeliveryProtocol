/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#include "mib.h"
#include "port.h"
#include "file_delivery_app.h"
#include "app_control.h"
#include "stdlib.h"


int create_ssp_server_drivers(FTP *app) {

    void *error = NULL;
    switch (app->remote_entity.type_of_network)
    {
        case posix_connectionless:
            error = ssp_connectionless_server_task(app);
            break;
        case posix_connection:
            error = ssp_connection_server_task(app);
            break;
        case csp_connectionless:
            error = ssp_csp_connectionless_server_task(app);
            break;
        case csp_connection:
            error = ssp_csp_connection_server_task(app);
            break;
        case generic:
            error = ssp_generic_server_task(app);
            break;
        default:
            ssp_printf("server couldn't start, 'type of network' not recognized\n");
            break;
    }
    if (error == NULL) {
        return -1;
    }
    return 0;

}

static int create_ssp_client_drivers(Client *client) {
    Remote_entity remote_entity = client->remote_entity;

    switch (remote_entity.type_of_network)
    {
        case posix_connectionless:
            client->client_handle = ssp_thread_create(STACK_ALLOCATION, ssp_connectionless_client_task, client);
            break;
        case posix_connection:
            client->client_handle = ssp_thread_create(STACK_ALLOCATION, ssp_connection_client_task, client);
            break;
        case csp_connectionless:
            client->client_handle = ssp_thread_create(STACK_ALLOCATION, ssp_csp_connectionless_client_task, client);
            break;
        case csp_connection:
            client->client_handle = ssp_thread_create(STACK_ALLOCATION, ssp_csp_connection_client_task, client);
            break;
        case generic:
            client->client_handle = ssp_thread_create(STACK_ALLOCATION, ssp_generic_client_task, client);
            break;
        default:
            ssp_printf("client couldn't start, 'type of network' not recognized\n");
            break;
    }
    if (client->client_handle == NULL) {
        return -1;
    }
    return 0;
}


static void make_default_data(){

    int error = ssp_mkdir("incomplete_requests");
    if (error < 0) {
        ssp_error("couldn't make directory incomplete_requests it either already exists or there is an issue\n");
    }

    error = ssp_mkdir("mib");
    if (error < 0) {
        ssp_error("couldn't make directory mib it either already exists or there is an issue\n");
    }
    const char *peer_file_sat = "{\n\
    \"cfdp_id\": 1,\n\
    \"UT_address\" : 1,\n\
    \"UT_port\" : 20,\n\
    \"type_of_network\" : 3,\n\
    \"default_transmission_mode\" : 1,\n\
    \"MTU\" : 200,\n\
    \"total_round_trip_allowance\" : 10000,\n\
    \"async_NAK_interval\" : 1000,\n\
    \"transaction_inactivity_limit\" : 1500,\n\
    \"async_report_interval\" : 123,\n\
    \"immediate_nak_mode_enabled\" : 123,\n\
    \"prompt_transmission_interval\" : 123,\n\
    \"disposition_of_incomplete\" : 123,\n\
    \"CRC_required\" : 123,\n\
    \"keep_alive_discrepancy_limit\" : 123,\n\
    \"positive_ack_timer_expiration_limit\" : 123,\n\
    \"nak_timer_expiration_limit\" : 123,\n\
    \"async_keep_alive_interval\" : 123,\n\
    \"one_way_light_time\" : 123\n\
}";

    const char *peer_file_ground_station = "{\n\
    \"cfdp_id\": 10,\n\
    \"UT_address\" : 10,\n\
    \"UT_port\" : 1,\n\
    \"type_of_network\" : 3,\n\
    \"default_transmission_mode\" : 1,\n\
    \"MTU\" : 200,\n\
    \"total_round_trip_allowance\" : 10000,\n\
    \"async_NAK_interval\" : 1000,\n\
    \"transaction_inactivity_limit\" : 1500,\n\
    \"async_keep_alive_interval\" : 123,\n\
    \"async_report_interval\" : 123,\n\
    \"immediate_nak_mode_enabled\" : 123,\n\
    \"prompt_transmission_interval\" : 123,\n\
    \"disposition_of_incomplete\" : 123,\n\
    \"CRC_required\" : 0,\n\
    \"keep_alive_discrepancy_limit\" : 8,\n\
    \"positive_ack_timer_expiration_limit\" : 123,\n\
    \"nak_timer_expiration_limit\" : 123,\n\
    \"one_way_light_time\" : 123\n\
}";

    int fd = ssp_open("mib/peer_1.json", SSP_O_CREAT | SSP_O_RDWR | SSP_O_TRUNC);
    if (fd < 0) {
        if (fd == SSP_EEXIST) {
            ssp_error("file exists\n");
        }
        else
            ssp_error("couldn't create default peer_0.json it either already exists or there is an issue\n");
    } else {
        error = ssp_write(fd, peer_file_sat, strnlen(peer_file_sat, 1000));
        if (error < 0) {
            ssp_error("couldn't write default file\n");
        }
    }

    fd = ssp_open("mib/peer_10.json", SSP_O_CREAT | SSP_O_RDWR | SSP_O_TRUNC);
    if (fd < 0) {
        if (fd == SSP_EEXIST) {
            ssp_error("file exists\n");
        }
        else
            ssp_error("couldn't create default peer_10.json it either already exists or there is an issue\n");
    } else {
        error = ssp_write(fd, peer_file_ground_station, strnlen(peer_file_ground_station, 1000));
        if (error < 0) {
            ssp_error("couldn't write default file\n");
        }
    }
}

int init_ftp(uint32_t my_cfdp_address, FTP *app) {
    int error = 0;
    

    //sanitize everything but the server_handle in case of race condition which sets the handler first.
    void *handler = app->server_handle;
    memset(app, 0, sizeof(FTP));
    app->server_handle = handler;

    make_default_data();
    

    Remote_entity remote_entity;
    memset(&remote_entity, 0, sizeof(Remote_entity));

    error = get_remote_entity_from_json(&remote_entity, my_cfdp_address);
    if (error == -1) {
        ssp_error("can't get configuration data, can't start server failed to start ftp.\n");
        return -1;
    }

    app->packet_len = remote_entity.mtu;
    app->buff = ssp_alloc(1, app->packet_len);
    app->transaction_sequence_number = rand() % 255;
    
    if (app->buff == NULL) {
        ssp_free(app);
        return -1;
    }

    app->my_cfdp_id = my_cfdp_address;
    app->close = false;
    app->remote_entity = remote_entity;

    app->active_clients = linked_list();
    if (app->active_clients == NULL) {
        ssp_free(app->buff);
        ssp_free(app);
        return -1;
    }

    app->request_list = linked_list();
    if (app->request_list == NULL){
        ssp_free(app->buff);
        ssp_free(app);
        app->active_clients->freeOnlyList(app->active_clients);
        return -1;
    }

    app->current_request = NULL;
    ssp_printf("initializing ftp server task \n");
    app->initialized = true;
    return create_ssp_server_drivers(app);
}


static void* init_ftp_task(void *app){
    FTP *ap = (FTP *) app;
    int error = init_ftp(ap->my_cfdp_id, ap);
    if (error < 0) {
        //task failed to start destroy task/thread
    }
    return NULL;
}

void *create_ftp_task(uint32_t cfdp_id, FTP *app){

    app->my_cfdp_id = cfdp_id;
    void *handler = ssp_thread_create(STACK_ALLOCATION, init_ftp_task, app);
    app->server_handle = handler;
   
    return handler;
}


Client *init_client(uint32_t dest_cfdp_id, uint32_t my_cfdp_id){

    Remote_entity remote_entity;
    int error = get_remote_entity_from_json(&remote_entity, dest_cfdp_id);
    if (error < 0) {
        ssp_error("couldn't get client remote_entity from mib\n");
        return NULL;
    }

    Client *client = ssp_alloc(sizeof(Client), 1);
    if (client == NULL)
        return NULL;
    
    client->request_list = linked_list();
    if (client->request_list == NULL) {
        ssp_free(client);
        return NULL;
    }

    client->packet_len = remote_entity.mtu;
    client->buff = ssp_alloc(1, remote_entity.mtu);
    if (client->buff == NULL){
        ssp_free(client);
        client->request_list->freeOnlyList(client->request_list);
        return NULL;
    }

    client->close = 0;
    client->remote_entity = remote_entity;
    client->lock = NULL;

    get_header_from_mib(&client->pdu_header, remote_entity, my_cfdp_id);
    
    client->current_request = NULL;
    return client;
}

Client *ssp_client(uint32_t cfdp_id, FTP *app) {

    
    Client *client = init_client(cfdp_id, app->my_cfdp_id);
    if (client == NULL) {
        return NULL;
    }

    int error = create_ssp_client_drivers(client);
    if (error < 0) {
        ssp_cleanup_client(client);
        return NULL;
    }

    client->app = app;
    error = app->active_clients->insert(app->active_clients, client, cfdp_id);
    if (error == 0) {
        ssp_cleanup_client(client);
        ssp_printf("failed to add client to list of existing clients\n");
        return NULL;
    }
    return client;
}
