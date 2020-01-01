

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


//returns total space taken up in the packet from the added lv
uint16_t copy_lv_to_buffer(char *buffer, LV *lv){
    uint16_t packet_index = 0;
    buffer[packet_index] = lv->length;
    packet_index++;
    memcpy(&buffer[packet_index], lv->value, lv->length);
    packet_index += lv->length;
    return packet_index;
}

void free_lv(LV *lv) {
    ssp_free(lv->value);
    ssp_free(lv);
}

LV *create_lv(int size, void *value) {

    LV *lv = ssp_alloc(1, sizeof(LV));
    lv->value = ssp_alloc(size, sizeof(char));
    

    memcpy(lv->value, value, size);
    lv->length = size;

    return lv;
}

Message *create_message(uint8_t type) {

    Message *message = ssp_alloc(1, sizeof(Message));    
    message->header.message_id_cfdp = ssp_alloc(5, sizeof(char));
    memcpy(message->header.message_id_cfdp, "cfdp", 5);
    message->header.message_type = type;
    return message;
}


//lv is what we copy into, packet is the buffer, and start is where in the buffer
//we start copying the lv to
LV *copy_lv_from_buffer(char *packet, uint32_t start) {
    uint8_t len = packet[start];
    return create_lv(len, &packet[start + 1]);
}   


void ssp_free_message(void *params) {

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
        req->messages_to_user->free(req->messages_to_user, ssp_free_message);
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
    
    req->procedure = none;
    req->paused = true;
    reset_timeout(&req->timeout);
    
    req->res.msg = req->buff;

    req->messages_to_user = linked_list();
    checkAlloc(req->buff,  1);
    return req;
}



//starts a new client, adding it to app->active_clients, as well as 
//starting a new request and adding it to the client, returns a pointer
//to the request
static Request *start_new_client_request(FTP *app, uint8_t dest_id) {

    //spin up a new client thread
    Client *client = (Client *) app->active_clients->find(app->active_clients, dest_id, NULL, NULL);


    if (client == NULL) {
        ssp_printf("Spinning up a new client thread\n");
        client = ssp_client(dest_id, app);
        app->active_clients->insert(app->active_clients, client, dest_id);
    } else {
        ssp_printf("adding request to existing client thread\n");
    }

    Request *req = init_request(client->packet_len);

    //build a request 
    req->transaction_sequence_number = app->transaction_sequence_number++;
    req->dest_cfdp_id = client->remote_entity->cfdp_id;
    req->pdu_header = get_header_from_mib(app->mib, client->remote_entity->cfdp_id, app->my_cfdp_id);
    req->res.packet_len = client->packet_len;
    req->packet_data_len = app->packet_len;
    
    client->request_list->insert(client->request_list, req, 0);

    return req;
}


/*NULL for source and destination filenames shall indicate that only Meta
data will be delivered. Side effect: add request to client request list
returns the request*/
Request *put_request(
            uint32_t dest_id,
            char *source_file_name,
            char *destination_file_name,
            uint8_t transmission_mode,
            FTP *app
            ) {

    Request *req;
    uint32_t file_size = 0;

    ssp_printf("trying to start new request\n");
    if (source_file_name == NULL || destination_file_name == NULL) {
        req = start_new_client_request(app, dest_id);
        req->transmission_mode = transmission_mode;
        req->procedure = sending_start;
        return req;
    }

    if (strnlen(source_file_name, MAX_PATH) == 0 || strnlen(destination_file_name, MAX_PATH) == 0) {
        ssp_printf("ERROR: no file names present in put request, if you want to just send messages, make both source and dest NULL\n");
        return NULL;
    }

    req = start_new_client_request(app, dest_id);

    file_size = get_file_size(source_file_name);
    if (file_size == 0)
        return NULL;

    req->file = create_file(source_file_name, false);
    req->file_size = file_size;

    req->transmission_mode = transmission_mode;
    req->procedure = sending_start;
    
    memcpy(req->source_file_name, source_file_name ,strnlen(source_file_name, MAX_PATH));
    memcpy(req->destination_file_name, destination_file_name, strnlen(destination_file_name, MAX_PATH));

    return req;
}

void start_request(Request *req){
    req->paused = false;
}

//Omission of source and destination filenames shall indicate that only Meta
//data will be delivered


Message_put_proxy *create_message_put_proxy(uint32_t beneficial_cfid, uint8_t length_of_id, char *source_name, char *dest_name, Request *req) {

    Message_put_proxy *proxy = ssp_alloc(1, sizeof(Message_put_proxy));
    proxy->destination_file_name = create_lv(strnlen(dest_name, MAX_PATH) + 1, dest_name);
    proxy->source_file_name = create_lv(strnlen(source_name, MAX_PATH) + 1, source_name);
    proxy->destination_id = create_lv(length_of_id, &beneficial_cfid);
    return proxy;
}

//beneficial_cfid is the destination id that the proxy will send to, length_of_id is in octets (or bytes)
int add_proxy_message_to_request(uint32_t beneficial_cfid, uint8_t length_of_id, char *source_name, char *dest_name, Request *req) {

    Message *message = create_message(PROXY_PUT_REQUEST);
    message->value = create_message_put_proxy(beneficial_cfid, length_of_id, source_name, dest_name, req);
    req->messages_to_user->push(req->messages_to_user, message, 0);

    return 1;
}


void print_request_state(Request *req) {

    ssp_printf("----------------Transaction %d---------------\n", req->transaction_sequence_number);
    ssp_printf("local_entity stats: \n");
    ssp_printf("EOF_recv indication %d\n", req->local_entity->EOF_recv_indication);
    ssp_printf("EOF_sent indication %d\n", req->local_entity->EOF_sent_indication);
    ssp_printf("Metadata_recv indication %d\n", req->local_entity->Metadata_recv_indication);
    ssp_printf("Metadata_sent indication %d\n", req->local_entity->Metadata_sent_indication);
    
    ssp_printf("Resume indication %d\n", req->local_entity->resumed_indication);
    ssp_printf("Suspended indication %d\n", req->local_entity->suspended_indication);
    ssp_printf("Transaction finished indication %d\n", req->local_entity->transaction_finished_indication);
    print_request_procedure(req);
    ssp_printf("---------------------------------------------\n");
}


void print_request_procedure(Request *req){

    ssp_printf("current procedure: ");
    switch (req->procedure)
    {
        case sending_eof: 
            ssp_printf("sending_eof\n");
            break;

        case sending_data:
            ssp_printf("sending_data\n");
            break;

        case sending_put_metadata:
            ssp_printf("sending_put_metadata\n");
            break;

        case sending_finished:
            ssp_printf("sending_finished\n");
            break;

        case sending_start:
            ssp_printf("sending_start\n");
            break;

        case clean_up: // will close the request happens in the previous functions
            ssp_printf("clean_up\n");
            break;

        case none:
            ssp_printf("none\n");
            break;

        default:
            break;
    }
}