
/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------

                                    USER STUFF
                                    aka: request from person

------------------------------------------------------------------------------*/
#include "port.h"
#include "requests.h"
#include "utils.h"
#include "types.h"
#include "filesystem_funcs.h"
#include "file_delivery_app.h"
#include "mib.h"


//returns total space taken up in the packet from the added lv
uint16_t copy_lv_to_buffer(char *buffer, LV lv) {
    uint16_t packet_index = 0;
    buffer[packet_index] = lv.length;
    packet_index++;
    ssp_memcpy(&buffer[packet_index], lv.value, lv.length);
    packet_index += lv.length;
    return packet_index;
}

void free_lv(LV lv) {
    ssp_free(lv.value);
}

//this should return -1 on alloc fail
void create_lv(LV *lv, int len, void *value) {

    lv->value = ssp_alloc(len, sizeof(char));
    ssp_memcpy(lv->value, value, len);
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
    ssp_memcpy(message->header.message_id_cfdp, "cfdp", 5);
    message->header.message_type = type;
    return message;
}




/*------------------------------------------------------------------------------
        Messages (additional minor requests, things like mv files)
------------------------------------------------------------------------------*/

//this will turn a incoming request to a request that will go out later
int init_cont_partial_request(Message_cont_part_request *p_cont, char *buff, uint32_t buff_len) {

    //don't need these params, just using this function for the req
    Request *req = init_request(buff, buff_len);
    if (req == NULL)
        return -1;
    
    uint32_t dest_id = p_cont->destination_id;
    uint64_t trans_num = p_cont->transaction_id;
    uint32_t src_id = p_cont->originator_id;

    int error = get_req_from_file(src_id, trans_num, dest_id, req);
    if (error < 0) {
        ssp_error("couldn't get request from file system\n");
        ssp_cleanup_req(req);
        return error;
    }
    Request old_request = *req;

    req->dest_cfdp_id = dest_id;
    req->res.addr = NULL;

    error = get_remote_entity_from_json(&req->remote_entity, dest_id);
    if (error < 0) {
        ssp_error("couldn't get remote config from file system\n");
        ssp_cleanup_req(req);
        return error;
    }
    req->local_entity.EOF_sent_indication = req->local_entity.EOF_recv_indication;
    req->local_entity.Metadata_sent_indication = req->local_entity.Metadata_recv_indication;
    req->local_entity.resumed_indication = 0;
    req->local_entity.suspended_indication = 0;
    req->local_entity.transaction_finished_indication = 0;
    req->my_cfdp_id = src_id;
    
    get_header_from_mib(&req->pdu_header, req->remote_entity, req->dest_cfdp_id);

    error = save_req_to_file(req);
    if (error < 0) {
        ssp_error("couldn't save req to file\n");
        ssp_cleanup_req(req);
        return -1;
    }
    //delete old file
    error = delete_saved_request(&old_request);
    if (error < 0) {
        ssp_error("couldn't remove saved request\n");
        ssp_cleanup_req(req);
        return -1;
    }
    ssp_cleanup_req(req);
    return 1;
}


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
    message->destination_id = beneficial_cfid;

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
                                    uint32_t originator_id,
                                    uint32_t transaction_id) {

    Message_cont_part_request *message = ssp_alloc(1, sizeof(Message_cont_part_request));
    if (message == NULL)
        return NULL;

    message->destination_id = beneficial_cfid;
    message->originator_id = originator_id;
    message->transaction_id = transaction_id;
    return message;
}

//beneficial_cfid is the destination id that the proxy will send to, originator
//is the sender's id
int add_cont_partial_message_to_request(uint32_t beneficial_cfid, 
                                    uint32_t originator_id,
                                    uint32_t transaction_id,
                                    Request *req){

    Message *message = create_message(CONTINUE_PARTIAL);
    if (message == NULL)
        return -1;

    message->value = create_message_cont_partial_request(beneficial_cfid, 
                                                    originator_id, 
                                                    transaction_id
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

}
static void ssp_free_proxy_cont_partial_request(Message_cont_part_request *message) {

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

        case CONTINUE_PARTIAL:
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
        ssp_free_file(req->file);
    
    if (req->messages_to_user->count > 0)
        req->messages_to_user->free(req->messages_to_user, ssp_free_message);
    else 
        req->messages_to_user->freeOnlyList(req->messages_to_user);

    ssp_free(req->res.addr);
    ssp_free(req);

}

Request *init_request_no_client() {

    Request *req = ssp_alloc(1, sizeof(Request));
    if (req == NULL)
        return NULL;
        
    memset(req, 0, sizeof(Request));

    req->file = NULL;
    req->procedure = none;
    req->paused = true;
    req->timeout_before_cancel = ssp_time_count();
    req->timeout_before_journal = ssp_time_count();

    req->messages_to_user = linked_list();
    if (req->messages_to_user == NULL) {
        ssp_free(req->buff);
        return NULL;
    }
    return req;
}


Request *init_request(char *buff, uint32_t buff_len) {

    Request *req = init_request_no_client();
    if (req == NULL)
        return NULL;

    req->buff_len = buff_len;
    req->buff = buff;
    req->res.msg = req->buff;

    return req;
}


Client *start_client(FTP *app, uint8_t dest_id) {


    while (!app->initialized);
     //spin up a new client thread
    Client *client = (Client *) app->active_clients->find(app->active_clients, dest_id, NULL, NULL);

    if (client == NULL) {
        ssp_printf("Spinning up a new client thread\n");
        client = ssp_client(dest_id, app);
        if (client == NULL) {
            ssp_printf("client is null, couln't spin up client thread");
            return NULL;
        }
        
    } else {
        ssp_printf("adding request to existing client thread\n");
    }
    return client;
}


//adds generic request to client, will set the pdu_header to new client
void add_request_to_client(Request *req, Client *client) {

    req->dest_cfdp_id = client->remote_entity.cfdp_id;
    req->pdu_header = client->pdu_header;
    req->my_cfdp_id = client->app->my_cfdp_id;
    req->buff = client->buff;
    req->buff_len = client->packet_len;
    client->request_list->insert(client->request_list, req, -1);

    //unlock if lock is present
    ssp_lock_give(client->lock);

}

int put_request_no_client(
    Request *req,
    char *source_file_name,
    char *destination_file_name,
    uint8_t transmission_mode,
    FTP *app) {

    uint32_t file_size = 0;
    
    //build a request
    req->my_cfdp_id = app->my_cfdp_id;
    req->transmission_mode = transmission_mode;
    req->procedure = sending_start;
    
    if (source_file_name == NULL && destination_file_name == NULL) {
        req->transaction_sequence_number = app->transaction_sequence_number++;
        return 0;
    }

    if (strnlen(source_file_name, MAX_PATH) == 0 || strnlen(destination_file_name, MAX_PATH) == 0) {
        ssp_printf("ERROR: no file names present in put request, if you want to just send messages, make both source and dest NULL\n");
        return -1;
    }

    bool exists = does_file_exist(source_file_name);
    if (exists == false) {
        ssp_printf("ERROR: File does not exist\n");
        return -1;
    }

    file_size = get_file_size(source_file_name);
    if (file_size == (uint32_t) -1) {
        ssp_printf("ERROR: couldn't get file size\n");
        return -1;
    }

    req->file = create_file(source_file_name, false);
    if (req->file == NULL) {
        ssp_printf("ERROR: couldn't create file\n");
        return -1;
    }

    //this could probably go into 'create_file'
    int error =  add_first_offset(req->file, req->file->total_size);
    if (error < 0) {
        ssp_free_file(req->file);
        return -1;
    }

    req->file_size = file_size;
    req->transaction_sequence_number = app->transaction_sequence_number++;
    ssp_memcpy(req->source_file_name, source_file_name ,strnlen(source_file_name, MAX_PATH));
    ssp_memcpy(req->destination_file_name, destination_file_name, strnlen(destination_file_name, MAX_PATH));
    return 0;
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


    Request *req = init_request_no_client();
    if (req == NULL) {
        ssp_error("couldn't init request");
        return NULL;
    } 

    if (put_request_no_client(req, source_file_name, destination_file_name, transmission_mode, app) < 0){
        ssp_error("couldn't configure request");
        return NULL;
    }

    Client *client = start_client(app, dest_id);
    if (client == NULL) {
        ssp_printf("client failed to start\n");
        return NULL;
    } 
    
    add_request_to_client(req, client);
    return req;
}

Request *get_request(
            uint32_t dest_id,
            char *source_file_name,
            char *destination_file_name,
            uint8_t transmission_mode,
            FTP *app){

    Request *req = init_request_no_client();
    put_request_no_client(req, NULL, NULL, transmission_mode, app);
    add_proxy_message_to_request(app->my_cfdp_id, 1, source_file_name, destination_file_name, req);

    Client *client = start_client(app, dest_id);
    if (client == NULL) {
        ssp_printf("client failed to start\n");
    } else 
        add_request_to_client(req, client);

    return req;
}
/*NULL for source and destination filenames shall indicate that only Meta
data will be delivered. Side effect: add request to client request list
returns the request*/

int schedule_request(Request *req, uint32_t dest_id, FTP *app) {
    req->dest_cfdp_id = dest_id;
    req->my_cfdp_id = app->my_cfdp_id;
    req->procedure = sending_start;
    int error = save_req_to_file(req);
    return error;
}

int schedule_put_request(
            uint32_t dest_id,
            char *source_file_name,
            char *destination_file_name,
            uint8_t transmission_mode,
            FTP *app
            ) {
    int error = 0;

    Request *req = init_request_no_client();
    if (req == NULL) {
        ssp_error("couldn't init request");
        return -1;
    } 

    
    error = put_request_no_client(req, source_file_name, destination_file_name, transmission_mode, app);
    if (error < 0){
        ssp_error("couldn't configure request");
        return -1;
    }

    error = schedule_request(req, dest_id, app);
    if (error < 0) {
        ssp_error("failed to schedule request");
        return -1;
    }
    return 0;
}
/*
static void clean_up_start_scheduled_requests(int fd, Request *req){

    if (req != NULL)
        ssp_cleanup_req(req);
    
    int error = ssp_close(fd);
    if (error < 0) {
        ssp_error("there was an error closing the file descriptor");
    }
}
*/

int start_scheduled_requests(uint32_t dest_id, FTP *app){

    char dir_name[MAX_PATH];
    ssp_snprintf(dir_name, MAX_PATH, "%s%u%s", "incomplete_requests/CFID:", dest_id, "_requests");
    ssp_printf("opening dir %s\n", dir_name);

    void *dir;
    char file[MAX_PATH];

    dir = ssp_opendir(dir_name);
    if(dir == NULL){
        ssp_error("Unable to open directory");
        return -1;
    }
    int error = 0;
    Request *req = NULL;
    Client *client = NULL;
    
    //adding +2 here because file->name is of max 256 size, and then we add a /. 
    char file_path[MAX_PATH + 2];

    client = start_client(app, dest_id);
    if (client == NULL) {
        ssp_error("couldn't start new request");
        return -1;
    }

    while( (ssp_readdir(dir, file)) )
    {
        if (strncmp(file, ".", 1) == 0 || strncmp(file, "..", 2) == 0)
            continue;

        ssp_snprintf(file_path, sizeof(file_path), "%s/%s", dir_name, file);

        req = init_request_no_client();
        if (req == NULL) {
            ssp_cleanup_req(req);
            ssp_error("couldn't init request");
            continue;
        } 

        error = get_request_from_json(req, file_path);
        if (error < 0) {
            ssp_cleanup_req(req);
            ssp_error("couldn't read in json request");
            continue;
        }

        add_request_to_client(req, client);
    }

    ssp_closedir(dir);
    return 0;
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

    ssp_printf("----------------Printing Message---------------\n");
    ssp_printf("Message type: %d\n", m->header.message_type);
    ssp_printf("id: %s\n", m->header.message_id_cfdp);
    Message_put_proxy *proxy;

    if (m->header.message_type == PROXY_PUT_REQUEST) {
        proxy = (Message_put_proxy *)m->value;
        ssp_printf("Message type: PROXY_PUT_REQUST\n");
        ssp_printf("dest filename: %s\n", proxy->destination_file_name.value);
        ssp_printf("source filename: %s\n", proxy->source_file_name.value);
        ssp_printf("id: %llu\n", proxy->destination_id);

    }

}

void print_request_state(Request *req) {

    ssp_printf("----------------Transaction %llu---------------\n", req->transaction_sequence_number);
    
    ssp_printf("local_entity id and stats: \n");

    ssp_printf("destination id %d: \n", req->dest_cfdp_id);
    ssp_printf("EOF_recv indication %d\n", req->local_entity.EOF_recv_indication);
    ssp_printf("EOF_sent indication %d\n", req->local_entity.EOF_sent_indication);
    ssp_printf("Metadata_recv indication %d\n", req->local_entity.Metadata_recv_indication);
    ssp_printf("Metadata_sent indication %d\n", req->local_entity.Metadata_sent_indication);
    ssp_printf("Resume indication %d\n", req->local_entity.resumed_indication);
    ssp_printf("Suspended indication %d\n", req->local_entity.suspended_indication);
    ssp_printf("Transaction finished indication %d\n", req->local_entity.transaction_finished_indication);
    
    if (req->file != NULL) {
        ssp_printf("checksum received = %u checksum calculated = %u\n", req->file->eof_checksum, req->file->partial_checksum);
        ssp_printf("offset list count %d\n", req->file->missing_offsets->count);
    }
    print_request_procedure(req);
    
    ssp_printf("current message count %d\n", req->messages_to_user->count);
    ssp_printf("current messages: \n");
    req->messages_to_user->iterate(req->messages_to_user, print_messages_callback, NULL);
    
    ssp_printf("request header destination id: %d\n", req->pdu_header.destination_id);
    ssp_printf("request header source id: %d\n", req->pdu_header.source_id);
    ssp_printf("request header crc flag: %d\n", req->pdu_header.CRC_flag);
    ssp_printf("request header direction: %d\n", req->pdu_header.direction);
    ssp_printf("request header length of Ids: %d\n", req->pdu_header.length_of_entity_IDs);
    ssp_printf("request header PDU_data_field_len: %d\n", req->pdu_header.PDU_data_field_len);
    ssp_printf("request header pdu type: %d\n", req->pdu_header.PDU_type);
    ssp_printf("request header transaction_seq_num_len: %d\n", req->pdu_header.transaction_seq_num_len);
    ssp_printf("request header transaction_sequence_number: %llu\n", req->pdu_header.transaction_sequence_number);
    ssp_printf("request header transmission_mode: %d\n", req->pdu_header.transmission_mode);
    ssp_printf("request header version: %d\n", req->pdu_header.version);
    ssp_printf("---------------------------------------------\n");
}


void print_request_procedure(Request *req){

    ssp_printf("current procedure: ");
    switch (req->procedure)
    {
      
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


void print_res(Response res){

    ssp_printf("addr %d\n", res.addr);
    ssp_printf("msg %d\n", res.msg);
    ssp_printf("packet_len %d\n", res.packet_len);
    ssp_printf("sfd %d\n", res.sfd);
    ssp_printf("size_of_addr %d\n", res.size_of_addr);
    ssp_printf("transmission_mode %d\n", res.transmission_mode);
    ssp_printf("type_of_network %d\n", res.type_of_network);
}
