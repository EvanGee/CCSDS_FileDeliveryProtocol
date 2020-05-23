
/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------

                                    USER STUFF
                                    aka: request from person

------------------------------------------------------------------------------*/
#include "requests.h"
#include "port.h"
#include "utils.h"
#include "types.h"
#include "filesystem_funcs.h"
#include "file_delivery_app.h"


//returns total space taken up in the packet from the added lv
uint16_t copy_lv_to_buffer(char *buffer, LV lv){
    uint16_t packet_index = 0;
    buffer[packet_index] = lv.length;
    packet_index++;
    memcpy(&buffer[packet_index], lv.value, lv.length);
    packet_index += lv.length;
    return packet_index;
}

void free_lv(LV lv) {
    ssp_free(lv.value);
}

void create_lv(LV *lv, int len, void *value) {

    lv->value = ssp_alloc(len, sizeof(char));
    memcpy(lv->value, value, len);
    lv->length = len;
}

//lv is what we copy into, packet is the buffer, and start is where in the buffer
//we start copying the lv to
void copy_lv_from_buffer(LV *lv, char *packet, uint32_t start) {
    uint8_t len = packet[start];
    create_lv(lv, len, &packet[start + 1]);
    return;
}   


Message *create_message(uint8_t type) {

    Message *message = ssp_alloc(1, sizeof(Message));    
    if (message == NULL)
        return NULL;

    //message->header.message_id_cfdp = ssp_alloc(5, sizeof(char));
    memcpy(message->header.message_id_cfdp, "cfdp", 5);
    message->header.message_type = type;
    return message;
}




/*------------------------------------------------------------------------------
        Messages (additional minor requests, things like mv files)
------------------------------------------------------------------------------*/



//Omission of source and destination filenames shall indicate that only Meta
//data will be delivered
Message_put_proxy *
create_message_put_proxy(uint32_t beneficial_cfid, 
                        uint8_t length_of_id, 
                        char *source_name, 
                        char *dest_name) {

    Message_put_proxy *message = ssp_alloc(1, sizeof(Message_put_proxy));
    if (message == NULL)
        return NULL;

    create_lv(&message->destination_file_name, strnlen(dest_name, MAX_PATH) + 1, dest_name);
    create_lv(&message->source_file_name, strnlen(source_name, MAX_PATH) + 1, source_name);
    create_lv(&message->destination_id, length_of_id, &beneficial_cfid);
    return message;
}

//beneficial_cfid is the destination id that the proxy will send to, length_of_id is in octets (or bytes)
int add_proxy_message_to_request(uint32_t beneficial_cfid, 
                                uint8_t length_of_id, 
                                char *source_name, 
                                char *dest_name, 
                                Request *req) {

    Message *message = create_message(PROXY_PUT_REQUEST);
    if (message == NULL)
        return -1;

    message->value = create_message_put_proxy(beneficial_cfid, length_of_id, source_name, dest_name);
    if (message->value == NULL) {
        ssp_free(message);
        return -1;
    }

    req->messages_to_user->push(req->messages_to_user, message, 0);
    return 1;
}

Message_cont_part_request *
create_message_cont_partial_request(uint32_t beneficial_cfid, 
                                    uint8_t beneficial_id_length, 
                                    uint32_t originator_id,
                                    uint8_t originator_id_length,
                                    uint32_t transaction_id,
                                    uint8_t transaction_id_length) {

    Message_cont_part_request *message = ssp_alloc(1, sizeof(Message_cont_part_request));
    if (message == NULL)
        return NULL;

    create_lv(&message->destination_id, beneficial_id_length, &beneficial_cfid);
    create_lv(&message->originator_id, originator_id_length, &originator_id);
    create_lv(&message->transaction_id, transaction_id_length, &transaction_id);
    return message;
}

//beneficial_cfid is the destination id that the proxy will send to, originator
//is the sender's id
int add_cont_partial_message_to_request(uint32_t beneficial_cfid, 
                                    uint8_t beneficial_id_length, 
                                    uint32_t originator_id,
                                    uint8_t originator_id_length,
                                    uint32_t transaction_id,
                                    uint8_t transaction_id_length,
                                    Request *req){

    Message *message = create_message(PROXY_CONTINUE_PARTIAL);
    if (message == NULL)
        return -1;

    message->value = create_message_cont_partial_request(beneficial_cfid, 
                                                    beneficial_id_length, 
                                                    originator_id, 
                                                    originator_id_length,
                                                    transaction_id,
                                                    transaction_id_length
                                                    );
    if (message->value == NULL) {
        ssp_free(message);
        return -1;
    }

    req->messages_to_user->push(req->messages_to_user, message, 0);
    return 1;
}


static void ssp_free_put_proxy_message(Message_put_proxy* message) {

    free_lv(message->destination_file_name);
    free_lv(message->source_file_name);
    free_lv(message->destination_id);

}
static void ssp_free_proxy_cont_partial_request(Message_cont_part_request *message) {

    free_lv(message->destination_id);
    free_lv(message->originator_id);
    free_lv(message->transaction_id);
}

void ssp_free_message(void *params) {

    Message *message = (Message*) params;
    Message_put_proxy* proxy_request;
    Message_cont_part_request* proxy_cont_partial_request;

    switch (message->header.message_type)
    {
        case PROXY_PUT_REQUEST:
            proxy_request = (Message_put_proxy *) message->value;
            ssp_free_put_proxy_message(proxy_request);
            break;

        case PROXY_CONTINUE_PARTIAL:
            proxy_cont_partial_request = (Message_cont_part_request *) message->value;
            ssp_free_proxy_cont_partial_request(proxy_cont_partial_request);
            break;
        default:
            break;
    }
    ssp_free(message->value);
    ssp_free(message);
}

/*------------------------------------------------------------------------------
        Requests (major functions to initialize requests
------------------------------------------------------------------------------*/

void ssp_cleanup_req(void *request) {

    if (request == NULL)
        return;

    Request *req = (Request *) request;

    if (req->file != NULL)
        free_file(req->file);
    
    if (req->messages_to_user->count > 0)
        req->messages_to_user->free(req->messages_to_user, ssp_free_message);
    else 
        req->messages_to_user->freeOnlyList(req->messages_to_user);

    ssp_free(req->res.addr);
    ssp_free(req);

}



Request *init_request(char *buff, uint32_t buff_len) {

    Request *req = ssp_alloc(1, sizeof(Request));
    if (req == NULL)
        return NULL;

    req->file = NULL;
    req->buff_len = buff_len;
    req->buff = buff;
    req->procedure = none;
    req->paused = true;
    req->timeout = ssp_time_count();
    req->res.msg = req->buff;

    req->messages_to_user = linked_list();
    if (req->messages_to_user == NULL) {
        ssp_free(req->buff);
        return NULL;
    }
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

    Request *req = init_request(client->buff, client->packet_len);
    
    //build a request 
    req->transaction_sequence_number = app->transaction_sequence_number++;
    req->dest_cfdp_id = client->remote_entity.cfdp_id;
    req->pdu_header = client->pdu_header;
    req->res.packet_len = client->packet_len;
    
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

    bool exists = does_file_exist(source_file_name);
    if (exists == false) {
        ssp_printf("ERROR: File does not exist\n");
        return NULL;
    }

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
    if (req == NULL) {
        ssp_printf("ERROR: couldn't start request\n");
        return;
    }
    ssp_printf("started request\n");
    req->paused = false;
}

static void print_messages_callback(Node *node, void *element, void *args) {
    
    Message *m = (Message*) element;

    ssp_printf("Message type: %d\n", m->header.message_type);
    ssp_printf("id: %s\n", m->header.message_id_cfdp);
    Message_put_proxy *proxy;

    if (m->header.message_type == PROXY_PUT_REQUEST) {
        proxy = (Message_put_proxy *)m->value;
        ssp_printf("Message type: PROXY_PUT_REQUST\n");
        ssp_printf("dest filename: %s\n", proxy->destination_file_name.value);
        ssp_printf("source filename: %s\n", proxy->source_file_name.value);
        ssp_printf("id lendth: %d\n", proxy->destination_id.length);
        ssp_printf("id: %d\n", *(uint8_t*) proxy->destination_id.value);

    }

}

void print_request_state(Request *req) {

    ssp_printf("----------------Transaction %d---------------\n", req->transaction_sequence_number);
    ssp_printf("local_entity stats: \n");
    ssp_printf("EOF_recv indication %d\n", req->local_entity.EOF_recv_indication);
    ssp_printf("EOF_sent indication %d\n", req->local_entity.EOF_sent_indication);
    ssp_printf("Metadata_recv indication %d\n", req->local_entity.Metadata_recv_indication);
    ssp_printf("Metadata_sent indication %d\n", req->local_entity.Metadata_sent_indication);
    ssp_printf("Resume indication %d\n", req->local_entity.resumed_indication);
    ssp_printf("Suspended indication %d\n", req->local_entity.suspended_indication);
    ssp_printf("Transaction finished indication %d\n", req->local_entity.transaction_finished_indication);
    print_request_procedure(req);
    
    ssp_printf("current messages: \n");
    req->messages_to_user->iterate(req->messages_to_user, print_messages_callback, NULL);
    
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