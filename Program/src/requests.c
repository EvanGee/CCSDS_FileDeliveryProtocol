

/*------------------------------------------------------------------------------

                                    USER STUFF
                                    aka: request from person

------------------------------------------------------------------------------*/
#include "stdint.h"
#include "requests.h"
#include "port.h"
#include "utils.h"
#include <string.h>
#include "tasks.h"
#include "types.h"
#include "filesystem_funcs.h"
#include "mib.h"
#include <stdbool.h>
#include "file_delivery_app.h"

void ssp_cleanup_req(void *request) {

    if (request == NULL)
        return;

    Request *req = (Request *) request;

    if (req->file != NULL)
        free_file(req->file);
    if (req->pdu_header != NULL)
        ssp_cleanup_pdu_header(req->pdu_header);
    if (req->source_file_name != NULL)  
        ssp_free(req->source_file_name);
    if (req->destination_file_name != NULL)
        ssp_free(req->destination_file_name);
    if (req->buff != NULL)
        ssp_free(req->buff);
    if (req->res.addr != NULL)
        ssp_free(req->res.addr);
    if (req->local_entity != NULL)
        ssp_free(req->local_entity);
    if (req != NULL)
        ssp_free(req);

}


Request *init_request(uint32_t buff_len) {

    Request *req = ssp_alloc(1, sizeof(Request));

    req->source_file_name = ssp_alloc(MAX_PATH, sizeof(char));
    checkAlloc(req->source_file_name, 1);

    req->destination_file_name = ssp_alloc(MAX_PATH, sizeof(char));
    checkAlloc(req->destination_file_name,  1);

    req->local_entity = ssp_alloc(1, sizeof(Local_entity));
    req->file = NULL;
    req->buff_len = buff_len;
    req->buff = ssp_alloc(buff_len, sizeof(char));
    req->res.msg = req->buff;
    req->procedure = none;
    checkAlloc(req->buff,  1);
    return req;
}

//Omission of source and destination filenames shall indicate that only Meta
//data will be delivered
Request *put_request(
            uint32_t dest_id,
            char *source_file_name,
            char *destination_file_name,
            uint8_t transmission_mode,
            FTP *app
            ) {

    uint32_t file_size = get_file_size(source_file_name);
    Client *client;

    if (file_size == -1)
        return NULL;

    client = (Client *) app->active_clients->find(app->active_clients, dest_id, NULL, NULL);
    if (client == NULL) {
        client = ssp_client(dest_id, app);
        app->active_clients->insert(app->active_clients, client, dest_id);
    }

    //give the client a new request to perform
    Request *req = init_request(client->packet_len);
    req->file = create_file(source_file_name, 0);

    //build a request 
    req->transaction_sequence_number = app->transaction_sequence_number++;

    //enumeration
    req->procedure = sending_put_metadata;
    req->paused = true;
    req->dest_cfdp_id = client->remote_entity->cfdp_id;
    req->file_size = file_size;
    
    memcpy(req->source_file_name, source_file_name ,strnlen(source_file_name, MAX_PATH));
    memcpy(req->destination_file_name, destination_file_name, strnlen(destination_file_name, MAX_PATH));

    req->transmission_mode = transmission_mode;
    req->res.addr = ssp_alloc(sizeof(uint64_t), 1);

    client->request_list->insert(client->request_list, req, 0);
    
    return req;
}


/*
//Omission of source and destination filenames shall indicate that only Meta
//data will be delivered


int add_proxy_to_request(uint32_t beneficial_cfid,  Request *req) {

}
*/