
#include "mib.h"
#include "protocol_handler.h"
#include "port.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "file_delivery_app.h"
#include "tasks.h"
#include <stdio.h>
#include <arpa/inet.h>
#include "utils.h"


FTP *init_ftp(uint32_t my_cfdp_address) {

    
    Remote_entity remote_entity;
    int error = get_remote_entity_from_json(&remote_entity, my_cfdp_address);
    if (error == -1) {
        ssp_error("couldn't start server\n");
        return NULL;
    }

    if (remote_entity.type_of_network == csp) {
        
        printf("Initialising CSP\r\n");

        /* Init CSP with address MY_ADDRESS */
        csp_init(remote_entity.UT_address);

        /* Init buffer system with 10 packets of maximum PACKET_LEN bytes each */
        csp_buffer_init(10, PACKET_LEN);

        /* Start router task with 500 word stack, OS task priority 1 */
        csp_route_start_task(500, 1);
    }
    
    FTP *app = ssp_alloc(sizeof(FTP), 1);
    if (app == NULL) 
        return NULL;
    
    app->packet_len = PACKET_LEN;
    app->my_cfdp_id = my_cfdp_address;
    app->close = false;
    app->remote_entity = remote_entity;

    app->active_clients = linked_list();
    if (app->active_clients == NULL) 
        return NULL;

    app->request_list = linked_list();
    if (app->request_list == NULL) 
        return NULL;

    app->current_request = NULL;
    ssp_server(app);
    return app;
}



void ssp_server(FTP *app) {

    if (app->remote_entity.type_of_network == posix && app->remote_entity.default_transmission_mode == UN_ACKNOWLEDGED_MODE) {
        app->server_handle = ssp_thread_create(STACK_ALLOCATION, ssp_connectionless_server_task, app);

    } else if(app->remote_entity.type_of_network == posix && app->remote_entity.default_transmission_mode == ACKNOWLEDGED_MODE) {
        app->server_handle = ssp_thread_create(STACK_ALLOCATION, ssp_connection_server_task, app);

    } else if (app->remote_entity.type_of_network == csp && app->remote_entity.default_transmission_mode == UN_ACKNOWLEDGED_MODE) {
        app->server_handle = ssp_thread_create(STACK_ALLOCATION, ssp_csp_connectionless_server_task, app);

    } else if (app->remote_entity.type_of_network == csp && app->remote_entity.default_transmission_mode == ACKNOWLEDGED_MODE) {
        app->server_handle = ssp_thread_create(STACK_ALLOCATION, ssp_csp_connection_server_task, app);
    }
}

Client *ssp_client(uint32_t cfdp_id, FTP *app) {

    Remote_entity remote_entity;
    int error = get_remote_entity_from_json(&remote_entity, cfdp_id);
    if (error < 0) {
        ssp_error("couldn't get client remote_entity from mib\n");
        return NULL;
    }
    
    Client *client = ssp_alloc(sizeof(Client), 1);
    if (client == NULL)
        return NULL;
        
    client->current_request = NULL;
    client->request_list = linked_list();
    
    if (client->request_list == NULL) {
        ssp_free(client);
        return NULL;
    }

    client->packet_len = PACKET_LEN;
    client->remote_entity = remote_entity;

    get_header_from_mib(&client->pdu_header, remote_entity, app->my_cfdp_id);

    client->app = app;

    if (remote_entity.type_of_network == posix && remote_entity.default_transmission_mode == UN_ACKNOWLEDGED_MODE) {
        client->client_handle = ssp_thread_create(STACK_ALLOCATION, ssp_connectionless_client_task, client);

    } else if(remote_entity.type_of_network == posix && remote_entity.default_transmission_mode == ACKNOWLEDGED_MODE) {
        client->client_handle = ssp_thread_create(STACK_ALLOCATION, ssp_connection_client_task, client);

    } else if (remote_entity.type_of_network == csp && remote_entity.default_transmission_mode == ACKNOWLEDGED_MODE) {
        client->client_handle = ssp_thread_create(STACK_ALLOCATION, ssp_csp_connection_client_task, client);

    } else if (remote_entity.type_of_network == csp && remote_entity.default_transmission_mode == UN_ACKNOWLEDGED_MODE) {
        client->client_handle = ssp_thread_create(STACK_ALLOCATION, ssp_csp_connectionless_client_task, client);
    }


    return client;
}
