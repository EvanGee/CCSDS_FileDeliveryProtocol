

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


static void free_lv(LV *lv) {
    ssp_free(lv->value);
    ssp_free(lv);
}

static LV *create_lv(int size, void *value) {

    LV *lv = ssp_alloc(1, sizeof(LV));
    lv->value = ssp_alloc(size, sizeof(char));
    memcpy(lv->value, value, size);
    lv->length = size;
    return lv;
}


void free_message(void *params) {

    Message *message = (Message*) params;
    Message_put_proxy* proxy_request;
    
    switch (message->header.message_type)
    {
        case PROXY_PUT_REQUEST:
            proxy_request = (Message_put_proxy *) message->value;
            free_lv(proxy_request->destination_file_name);
            free_lv(proxy_request->source_file_name);
            free_lv(proxy_request->destination_id);

            break;
    
        default:
            break;
    }

    ssp_free(message->header.message_id_cfdp);
    ssp_free(message->value);
    ssp_free(message);
}

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

    if (req->messages_to_user->count > 0)
        req->messages_to_user->free(req->messages_to_user, free_message);
    else 
        req->messages_to_user->freeOnlyList(req->messages_to_user);

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

    req->messages_to_user = linked_list();
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

void start_request(Request *req){
    req->paused = false;
}

//Omission of source and destination filenames shall indicate that only Meta
//data will be delivered



//beneficial_cfid is the destination id that the proxy will send to, length_of_id is in octets (or bytes)
int add_proxy_message_to_request(uint32_t beneficial_cfid, uint8_t length_of_id, char *source_name, char *dest_name, Request *req) {

    Message *message = ssp_alloc(1, sizeof(Message));    

    message->header.message_id_cfdp = ssp_alloc(5, sizeof(char));
    memcpy(message->header.message_id_cfdp, "cfdp", 5);
    message->header.message_type = PROXY_PUT_REQUEST;

    Message_put_proxy *proxy = ssp_alloc(1, sizeof(Message_put_proxy));

    proxy->destination_file_name = create_lv(strnlen(dest_name, MAX_PATH), dest_name);
    proxy->source_file_name = create_lv(strnlen(source_name, MAX_PATH), source_name);
    proxy->destination_id = create_lv(length_of_id, &beneficial_cfid);

    message->value = proxy;    
    req->messages_to_user->push(req->messages_to_user, message, 0);

    return 1;
}
