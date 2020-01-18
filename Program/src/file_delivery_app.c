
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

    //Memory information base
    MIB *mib = init_mib();

    //setting host name for testing
    char *host_name = "127.0.0.1";
    uint32_t addr = 0;

    inet_pton(AF_INET, host_name, &addr);
    
    //adding new cfdp entities to management information base
    add_new_cfdp_entity(mib, 1, addr, 1111, posix, UN_ACKNOWLEDGED_MODE);

    add_new_cfdp_entity(mib, 2, addr, 1112, posix, UN_ACKNOWLEDGED_MODE); 
    add_new_cfdp_entity(mib, 7, addr, 1113, posix, UN_ACKNOWLEDGED_MODE); 

    add_new_cfdp_entity(mib, 3, 1, 1, csp, UN_ACKNOWLEDGED_MODE);   
    add_new_cfdp_entity(mib, 4, 2, 2, csp, UN_ACKNOWLEDGED_MODE);   

    add_new_cfdp_entity(mib, 5, 3, 3, csp, ACKNOWLEDGED_MODE);   
    add_new_cfdp_entity(mib, 6, 4, 4, csp, ACKNOWLEDGED_MODE);   

    
    Remote_entity remote_entity;
    int error = get_remote_entity_from_json(&remote_entity, my_cfdp_address);
    if (error == -1) {
        ssp_error("couldn't start server\n");
        return NULL;
    }

    //find server client in mib
    //Remote_entity* server_entity = mib->remote_entities->find(mib->remote_entities, my_cfdp_address, NULL, NULL);
    //if (server_entity == NULL) {
    //    ssp_printf("couldn't find your id in the information base\n");
    //}

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
    app->packet_len = PACKET_LEN;
    app->my_cfdp_id = my_cfdp_address;
    app->mib = mib;
    app->close = 0;
    app->remote_entity = remote_entity;
    app->active_clients = linked_list();
    app->request_list = linked_list();
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

    Client *client = ssp_alloc(sizeof(Client), 1);
    if (checkAlloc(client) < 0)
        return NULL;
        
    client->current_request = NULL;
    client->request_list = linked_list();
    client->packet_len = PACKET_LEN;

    
    //Remote_entity *remote_entity = ssp_alloc(1, sizeof(Remote_entity));
    Remote_entity remote_entity;
    int error = get_remote_entity_from_json(&remote_entity, cfdp_id);
    if (error < 0) {
        ssp_error("couldn't get client remote_entity from mib\n");
        return NULL;
    }
    
    client->remote_entity = remote_entity;
    client->pdu_header = get_header_from_mib(remote_entity, app->my_cfdp_id);
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
