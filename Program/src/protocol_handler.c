/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#include "mib.h"
#include "port.h"
#include "protocol_handler.h"
#include "packet.h"
#include "filesystem_funcs.h"
#include "requests.h"
#include "types.h"
#include "utils.h"
#include "app_control.h"

static void transasction_log(char *msg, uint64_t transaction_sequence_number){
    char log_message[2000];
    ssp_snprintf(log_message, sizeof(log_message), "%s"FMT64"|%s\n", "transaction:", transaction_sequence_number, msg);
    ssp_printf(log_message);
}

static void build_temperary_file(Request *req, uint32_t size) {

    ssp_snprintf(req->source_file_name, 75, "%s"FMT64"%s", "incomplete_requests/.temp_", req->transaction_sequence_number, ".jpeg");
    ssp_printf("haven't received metadata yet, building temperary file %s\n", req->source_file_name);
    req->file = create_temp_file(req->source_file_name, size);
}

static void send_ack(Request *req, Response res, unsigned int type){
    if (req->transmission_mode == UN_ACKNOWLEDGED_MODE)
        return;

    uint8_t start = build_pdu_header(req->buff, req->transaction_sequence_number, req->transmission_mode, 0, &req->pdu_header);
    build_ack(req->buff, start, type);
    ssp_sendto(res);
}

static void send_nak(Request *req, Response res, unsigned int type) {
    if (req->transmission_mode == UN_ACKNOWLEDGED_MODE)
        return;
    
    uint8_t start = build_pdu_header(req->buff, req->transaction_sequence_number, req->transmission_mode, 0, &req->pdu_header);
    build_nak_directive(req->buff, start, type);

    ssp_sendto(res);
}

static void resend_eof_ack(Request *req, Response res) {

    transasction_log("sending eof ack", req->transaction_sequence_number);
    send_ack(req,res, EOF_PDU);
    req->resent_eof++;
}

/*------------------------------------------------------------------------------

                                    Processing Packets

------------------------------------------------------------------------------*/

//for finding the struct in the list
struct request_search_params {
    uint32_t source_id;
    uint32_t transaction_sequence_number;
};

//for finding the struct in the list
static int find_request(void *element, void *args) {
    Request *req = (Request *) element;
    struct request_search_params *params = (struct request_search_params *) args;
    if (req->dest_cfdp_id == params->source_id && req->transaction_sequence_number == params->transaction_sequence_number){
      return 1;
    }

    //ssp_printf("cant find request req->dest_cfdp_id %d params->source_id %d transaction_sequence_number %d:%d\n", req->dest_cfdp_id, params->source_id,req->transaction_sequence_number, params->transaction_sequence_number );
    return 0;
}


 Request *new_incomming_request(uint32_t source_id, 
        uint32_t transmission_mode, 
        uint32_t transaction_sequence_number,
        Response res,
        FTP *app) {

        Remote_entity remote_entity;
        int error = get_remote_entity_from_json(&remote_entity, source_id);
        if (error < 0) {
            ssp_error("could not get remote entity for incoming packet \n");
            return NULL;
        }
        Pdu_header pdu_header;
        error = get_header_from_mib(&pdu_header, remote_entity, app->my_cfdp_id);
        if (error < 0) {
            ssp_printf("Couldn't make PDU HEADER IS NULL\n");
            return NULL;
        }

        Request *found_req = init_request(app->buff, app->packet_len);
        if (found_req == NULL) {
            ssp_error("could not get allocate for new request \n");
            return NULL;
        }
        transasction_log("incoming new request", transaction_sequence_number);

        //Make new request and add it
        found_req->transmission_mode = transmission_mode;
        found_req->transaction_sequence_number = transaction_sequence_number;
        found_req->dest_cfdp_id = source_id;
        found_req->pdu_header = pdu_header;
        found_req->my_cfdp_id = app->my_cfdp_id;
        found_req->remote_entity = remote_entity;
        found_req->procedure = none;

        found_req->res.addr = ssp_alloc(1, res.size_of_addr);
        
        if (found_req->res.addr == NULL) {
            ssp_cleanup_req(found_req);
            return NULL;
        }

        ssp_memcpy(found_req->res.addr, res.addr, res.size_of_addr);

        found_req->res.packet_len = remote_entity.mtu;
        found_req->res.sfd = res.sfd;
        found_req->res.transmission_mode = app->remote_entity.default_transmission_mode;
        found_req->res.type_of_network = app->remote_entity.type_of_network;
        found_req->res.msg = found_req->buff;
        found_req->res.size_of_addr = res.size_of_addr;

        found_req->paused = false;
        return found_req;
}


/*creates a request struct if there is none for the incomming request based on transaction sequence number or
finds the correct request struct and replaces req with the new pointer. Returns the possition in the packet 
where the data portion is, also sets incoming_pdu_header... returns -1 on fail*/

int process_pdu_header(char*packet, uint8_t is_server, Pdu_header *incoming_pdu_header, Response res, Request **req, List *request_list, FTP *app) {
    
    Pdu_header header;
    memset(&header, 0, sizeof(Pdu_header));

    int error = get_pdu_header_from_packet(packet, &header);
    *incoming_pdu_header = header;
    
    //ssp_printf("received packet is server %d\n", is_server);
    //ssp_print_header(&header);

    if (error < 0) {
        ssp_error("failed to get pdu header, bad formatting");
        return -1;
    }
    if (app->my_cfdp_id != header.destination_id){
        ssp_print_bits(packet, 12);
        ssp_printf("someone is sending packets here that are not for my id %u, dest_id: %u\n", app->my_cfdp_id, header.destination_id);
        ssp_print_header(&header);
        return -1;
    }

    uint16_t len = header.PDU_data_field_len;
    if (len > app->packet_len){
        ssp_printf("packet received %d that was too big for our buffer %d\n", len, app->packet_len);
        return -1;
    }

    //if packet is from the same request, don't' change current request
    Request *current_req = (*req);

    if (current_req != NULL && current_req->transaction_sequence_number == header.transaction_sequence_number && current_req->dest_cfdp_id == header.source_id){        
        return header.reserved_space_for_header;
    }

    //look for active request in list
    struct request_search_params params = {
        header.source_id,
        header.transaction_sequence_number,
    };

    Request *found_req = (Request *) request_list->find(request_list, -1, find_request, &params);

    //if server, create new request (can probably move this out of this function)
    if (found_req == NULL && is_server) 
    {
   
        found_req = new_incomming_request(header.source_id, 
            header.transmission_mode, 
            header.transaction_sequence_number,
            res,
            app);
            
        if (found_req == NULL) {
            ssp_error("could not create request for incomming transmission");
            return -1;
        }

        app->request_list->push(app->request_list, found_req, header.transaction_sequence_number);
    } 

    else if (found_req == NULL) {
        ssp_error("could not find request \n");
        return -1;
    }

    *req = found_req;
    
    return header.reserved_space_for_header;

}

// receives the offset
// writes offset to a file
// calclulates checksum
void process_data_packet(char *packet, uint32_t data_len, File *file) {

    if(file == NULL) {
        ssp_error("file struct is null, can't write to file");
        return;
    }

    uint32_t offset_start = get_data_offset_from_packet(packet);
    uint32_t packet_index = 4;

    //ssp_printf("received data packet offset:%d\n", offset_start);
    // size of 'offset' bytes in packet
    uint32_t offset_end = offset_start + data_len - packet_index;

    //ssp_printf("received offset %d:%d\n", offset_start, offset_end);

    if (!receive_offset(file, offset_start, offset_end)) {
        ssp_printf("throwing out packet\n");
        return;
    }

    uint32_t remaining_buffer_length = data_len - packet_index;
    int bytes = write_offset(file, &packet[packet_index], remaining_buffer_length, offset_start);
    if (bytes <= 0) {
        ssp_error("no new data was written\n");
        return;
    }

    file->partial_checksum += calc_check_sum(&packet[packet_index], bytes);

    if (file->missing_offsets->count == 0)
        return;
}


/*
typedef struct pdu_meta_data {
    //0 Record boundaries respeced (read as array of octets), 1 not respected (variable length)
    unsigned int segmentation_control : 1; 
    
    unsigned int reserved_bits: 7;

    //length of the file in octets, set all 0 for unbounded size
    uint32_t file_size;
    LV source_file_name;
    LV destination_file_name;

    
    //Options include:
    //    Filestore requests, 
    //    Messages to user.
    //    Fault Handler overrides.
    //    Flow Label. 

    TLV *options;

} Pdu_meta_data;
*/
uint32_t parse_metadata_packet(char *packet, uint32_t start, Request *req_to_fill) {

    
    memset(req_to_fill->source_file_name, 0, MAX_PATH);
    memset(req_to_fill->destination_file_name, 0, MAX_PATH);
    
    req_to_fill->segmentation_control = get_bits_from_protocol_byte(packet[start], 0, 0); 
    uint8_t packet_index = start + 1;

    uint32_t file_len = 0;
    memcpy(&file_len, &packet[packet_index], 4);

    req_to_fill->file_size = ssp_ntohl(file_len);
    packet_index += 4;

    uint8_t file_name_len = packet[packet_index];
    packet_index++;

    ssp_memcpy(req_to_fill->source_file_name, &packet[packet_index], file_name_len);
    packet_index += file_name_len;

    file_name_len = packet[packet_index];
    packet_index++;

    ssp_memcpy(req_to_fill->destination_file_name, &packet[packet_index], file_name_len);

    packet_index += file_name_len;

    return packet_index;
}


void process_messages(Request *req, FTP *app) {

    while (req->messages_to_user->count) {
        Message *message = req->messages_to_user->pop(req->messages_to_user);
        Message_put_proxy *p_put;
        Message_cont_part_request *p_cont;
        int error = 0;
        char *error_msg = "couldn't process messages: %s";

        //on failure, these will send back an error message to the requester
        switch (message->header.message_type)
        {
            case PROXY_PUT_REQUEST:
                    
                p_put = (Message_put_proxy *) message->value;
                ssp_printf("received proxy request for source file name: %s dest file name %s, to id %llu\n", 
                (char *)p_put->source_file_name.value,
                (char *)p_put->destination_file_name.value,
                p_put->destination_id);

                start_request(put_request(p_put->destination_id,
                (char *)p_put->source_file_name.value, 
                (char *)p_put->destination_file_name.value, req->remote_entity.default_transmission_mode, app));

                break;

            case CONTINUE_PARTIAL: { 
                p_cont = (Message_cont_part_request *) message->value;

                uint64_t dest_id = p_cont->destination_id;
                uint64_t orig_id = p_cont->originator_id;
                uint64_t tran_id = p_cont->transaction_id;
                
                ssp_printf("received message request to continue one way communication destination id %llu, originator id %llu, transaction id %llu\n",
                dest_id, orig_id, tran_id);
                
                if (orig_id != app->my_cfdp_id) {
                    ssp_printf(error_msg, "continue partial request, wrong originator ID");
                    return;
                }
                
                error = init_cont_partial_request(p_cont, app->buff, app->packet_len);
                if (error < 0)
                    ssp_printf(error_msg, "continue partial request\n");
                break;
            }

            default:
                ssp_printf("message type not recognized\n");
                break;
        }
        ssp_free_message(message); 
    }  

}


/*------------------------------------------------------------------------------

                                    Client 
                                    aka: handles responses from server

------------------------------------------------------------------------------*/

static void resend_finished_ack(Request *req, Response res) {
    transasction_log("sending finished ack", req->transaction_sequence_number);
    send_ack(req, res, FINISHED_PDU);
    req->resent_finished++;
}

static void send_put_metadata(Request *req, Response res) {

    uint32_t start = build_pdu_header(req->buff, req->transaction_sequence_number, req->transmission_mode, 0, &req->pdu_header);
    transasction_log("sending metadata pdu", req->transaction_sequence_number);
    start = build_put_packet_metadata(req->buff, start, req);

    req->local_entity.Metadata_sent_indication = true;
    ssp_sendto(res);
}

static void send_eof_pdu(Request *req, Response res) {
    uint32_t start = build_pdu_header(req->buff, req->transaction_sequence_number, req->transmission_mode, 0, &req->pdu_header);
    transasction_log("sending eof pdu", req->transaction_sequence_number);
    if (req->file_size == 0)
        build_eof_packet(req->buff, start, 0, 0);
    else 
        build_eof_packet(req->buff, start, req->file->total_size, req->file->partial_checksum);
    
    req->local_entity.EOF_sent_indication = true;
    ssp_sendto(res);
    return;
}

int create_data_burst_packets(char *packet, uint32_t start, File *file, uint32_t length) {

    if (file->next_offset_to_send >= file->total_size){
        return 0;
    }

    uint32_t packet_index = start;
    uint32_t size_of_offset_bytes = sizeof(uint32_t);

    int data_len = build_data_packet(packet, packet_index, length, file->next_offset_to_send, file);

    packet_index += size_of_offset_bytes;

    //number of bytes sent
    int bytes = data_len - size_of_offset_bytes;

    //calculate checksum for data packet, this is used to calculate in transit checksums
    file->partial_checksum += calc_check_sum(&packet[packet_index], bytes);
    
    //ssp_printf("sending packet data offset:size %d:%d\n", file->next_offset_to_send, file->next_offset_to_send + bytes);
    file->next_offset_to_send += bytes;
    
    if (file->next_offset_to_send == file->total_size) {
        ssp_printf("sending packet data offset_start:offset_end:total_size %d:%d:%d\n", file->next_offset_to_send - bytes, file->next_offset_to_send, file->total_size);
        return 1;
    }
    
    ssp_printf("sending packet data offset_start:offset_end:total_size %d:%d:%d\n", file->next_offset_to_send - bytes, file->next_offset_to_send, file->total_size);

    return 0;
}

struct cont_partial_params {
    uint32_t start;
    Response *res;
    Request *req;
};

static void continue_partials_send_partials(Node *node, void *element, void *args) {

    struct cont_partial_params *params = (struct cont_partial_params *) args;

    
    Request *req = params->req;
    Response *res = params->res;
    uint32_t start = params->start;
    Offset *o = (Offset *) element;

    int i = 0;
    for (i = o->start; i < o->end; i += res->packet_len) {
        req->file->next_offset_to_send = i;
        create_data_burst_packets(req->buff, start, req->file, res->packet_len);
        ssp_sendto(*res);
    }
    
}

static void continue_partials_start(Request *req, Response res, bool *close) {
    uint32_t start = build_pdu_header(req->buff, req->transaction_sequence_number, req->transmission_mode, 0, &req->pdu_header);
    int i = 0;

    for (i = 0; i < RESEND_META_TIMES; i++) {
        send_put_metadata(req, res);
    }

    
    struct cont_partial_params params = {
        start,
        &res,
        req,
    };
    
    //send all missing offsets
    req->file->missing_offsets->iterate(req->file->missing_offsets, continue_partials_send_partials, &params);
    
    for (i = 0; i < RESEND_EOF_TIMES; i++) {
        send_eof_pdu(req, res);
    }
    
    req->procedure = none;
}


static void acknowledged_start(Request *req, Response res, bool *close) {
    uint32_t start = build_pdu_header(req->buff, req->transaction_sequence_number, req->transmission_mode, 0, &req->pdu_header);

    send_put_metadata(req, res);

    if (req->file_size == 0 ){
        req->procedure = none;
        return;
    }

    while (!create_data_burst_packets(req->buff, start, req->file, res.packet_len)) {
        ssp_sendto(res);
        if (*close) {
            return;
        }
    }
    ssp_sendto(res);

    send_eof_pdu(req, res);
    req->procedure = none;
}

static void unacknowledged_start(Request *req, Response res, bool *close){

    uint32_t start = build_pdu_header(req->buff, req->transaction_sequence_number, req->transmission_mode, 0, &req->pdu_header);
    int i = 0;
    
    for (i = 0; i < RESEND_META_TIMES; i++) {
        send_put_metadata(req, res);
    }

    if (req->file_size == 0 ){
        req->procedure = none;
        return;
    }

    while (!create_data_burst_packets(req->buff, start, req->file, res.packet_len)) {
        ssp_sendto(res);
        if (*close) {
            return;
        }
    }

    ssp_sendto(res);

    for (i = 0; i < RESEND_EOF_TIMES; i++) {
        send_eof_pdu(req, res);
    }
    
    req->procedure = none;
}

//if no file attached to request, set procedure to none
static void start_sequence(Request *req, Response res, bool *close) {
    
    if (req->transmission_mode == UN_ACKNOWLEDGED_MODE) {
        unacknowledged_start(req, res, close);
        return;
    }
    acknowledged_start(req, res, close);
    //set timeout to 0, databurst can take a while, timeout should start after data burst
    reset_timeout(&req->timeout_before_cancel);
    
}


static int segment_offset_into_data_packets(char *packet, uint32_t start, uint32_t offset_start, uint32_t offset_end, Request *req, Response res){


    int i = 0;
    int error = 0; 

    //the segment length has to reduce the length of the segment by the 'offset' bytes in the data packet
    uint32_t segment_len = req->buff_len - start - sizeof(uint32_t);

    for (i = offset_start; i < offset_end; i+= segment_len) {

        if (offset_end - i < segment_len){
            segment_len = offset_end - i;
        }
        
        //ssp_printf("sending offset start %d to %d\n", i, i + segment_len);
        //ssp_printf("segment len %d\n",segment_len);

        error = build_data_packet(packet, start, req->buff_len, i, req->file);
        if (error < 0) {
            ssp_printf("couldn't create data packet for offset %d\n", i);
            continue;
        }
        ssp_sendto(res);
    }
    return 0;
}




int process_nak_pdu(char *packet, Request *req, Response res, Client *client){
    Pdu_nak nak;
    
    uint32_t packet_index = get_nak_packet(packet, &nak);
    uint32_t offset_start = 0;
    uint32_t offset_end = 0;
    //build new header for outgoing packets
    uint32_t outgoing_packet_index = build_pdu_header(req->buff, req->transaction_sequence_number, 0, 0, &client->pdu_header);
    int i = 0;

    ssp_printf("sending offset packet start %d offset end %d\n", nak.start_scope, nak.end_scope);
    //ssp_printf("number of segments requests %d\n", nak.segment_requests);
    
    for (i = 0; i < nak.segment_requests; i++){
        
        memcpy(&offset_start, &packet[packet_index], sizeof(uint32_t));
        offset_start = ssp_ntohl(offset_start);
        packet_index += 4;

        memcpy(&offset_end, &packet[packet_index], sizeof(uint32_t));
        offset_end = ssp_ntohl(offset_end);
        packet_index += 4;


        //ssp_printf("offset_start %d offset_end %d \n", offset_start, offset_end);

        segment_offset_into_data_packets(req->buff, outgoing_packet_index, offset_start, offset_end, req, res);
    }

    return 1;
}



//fills the current request with packet data, responses from servers
void parse_packet_client(char *packet, uint32_t packet_index, Response res, Request *req, Client* client) {
 
    uint8_t directive = packet[packet_index];
    packet_index += 1; 

    switch(directive) {
        case FINISHED_PDU:
            transasction_log("received finished pdu",  req->transaction_sequence_number);
            req->local_entity.transaction_finished_indication = true;
            resend_finished_ack(req, res);
            break;
        case NAK_PDU:
            transasction_log("received Nak pdu",  req->transaction_sequence_number);
            process_nak_pdu(&packet[packet_index], req, res, client);
            break;
        case ACK_PDU:

            switch (packet[packet_index])
            {
                case EOF_PDU:
                    transasction_log("received Eof ack",  req->transaction_sequence_number);
                    req->local_entity.EOF_recv_indication = true;
                    break;
            
                case META_DATA_PDU:
                    transasction_log("received meta_data ack",  req->transaction_sequence_number);
                    req->local_entity.Metadata_recv_indication = true;

                default:
                    break;
            }

            break;
        case NAK_DIRECTIVE:
            switch (packet[packet_index])
            {
                case META_DATA_PDU:
                    transasction_log("resending metadata pdu",  req->transaction_sequence_number);
                    send_put_metadata(req, res);
                    break;
            
                case EOF_PDU: 
                    transasction_log("resending eof pdu",  req->transaction_sequence_number);
                    send_eof_pdu(req, res);
                    break;
                
                default:
                    break;
            }

            break;
        default:
            break;
    }
}

//current user request, to send to server
void user_request_handler(Response res, Request *req, Client* client) {

    if (req == NULL || req->paused)
        return;
    
    switch (req->procedure)
    {
        case sending_nak_data:
            continue_partials_start(req, res, &client->close);
            break;

        case sending_start:
            start_sequence(req, res, &client->close);
            break;

        case clean_up: // will close the request happens in the previous functions
        case none:
            break;
        default:
            break;
    }
}
/*------------------------------------------------------------------------------

                                    SERVER SIDE
                                    aka: handles responses from remote

------------------------------------------------------------------------------*/

static void request_eof(Request *req, Response res) {
    transasction_log("sending eof nak pdu",  req->transaction_sequence_number);
    send_nak(req, res, EOF_PDU);
}

static void request_metadata(Request *req, Response res) {
    transasction_log("sending meta_data nak pdu",  req->transaction_sequence_number);
    send_nak(req, res, META_DATA_PDU);
}

static void request_data(Request *req, Response res) {
    uint8_t start = build_pdu_header(req->buff, req->transaction_sequence_number, 1, 0, &req->pdu_header);
    transasction_log("sending data nak pdu",  req->transaction_sequence_number);
    build_nak_packet(req->buff, start, req);
    ssp_sendto(res);
}

static void resend_finished_pdu(Request *req, Response res) {

    uint8_t start = build_pdu_header(req->buff, req->transaction_sequence_number, 1, 0, &req->pdu_header);
    transasction_log("sending finished pdu",  req->transaction_sequence_number);
    build_finished_pdu(req->buff, start);
    ssp_sendto(res);
    req->resent_finished++;   
}


//processes the eof packet, sets checksum, indication, and filesize.
void process_pdu_eof(char *packet, Request *req, Response res) {

    Pdu_eof eof_packet;
    get_eof_from_packet(packet, &eof_packet);
    
    if (req->file == NULL && req->local_entity.Metadata_recv_indication) {
        build_temperary_file(req, eof_packet.file_size);
    }

    req->local_entity.EOF_recv_indication = true;
    req->file->eof_checksum = eof_packet.checksum;
    req->file->total_size = eof_packet.file_size;
    
}



int process_file_request_metadata(Request *req) {

    char temp[75];
    if (req->file == NULL)
        req->file = create_file(req->destination_file_name, 1);

    else if (req->file->is_temp) {
        ssp_snprintf(temp, 75, "%s"FMT64"%s", "incomplete_requests/.temp_", req->transaction_sequence_number, ".jpeg");
        change_tempfile_to_actual(temp, req->destination_file_name, req->file_size, req->file);
        return 1;
    }

    if (req->file == NULL) {
        return -1;
    }
    
    int error =  add_first_offset(req->file, req->file_size);
    if (error < 0) {
        ssp_free_file(req->file);
        return -1;
    }

    return 1;
}

static void process_metadata(char *packet, uint32_t packet_index, Response res, Request *req, FTP *app) {

    req->local_entity.Metadata_recv_indication = true;

    packet_index = parse_metadata_packet(packet, packet_index, req);
    uint16_t data_len = get_data_length(packet);

    get_messages_from_packet(packet, packet_index, data_len, req);
    process_messages(req, app);
    
    send_ack(req, res, META_DATA_PDU);

    if (req->file_size != 0) 
        process_file_request_metadata(req);
    else {
        ssp_printf("just receiving messages, closing request\n");
        req->local_entity.EOF_recv_indication = true;
        //TODO this was set to clean_up on FreeRTOS because we didn't have a correct clock yet to, set to NONE when clock is right
        //it kind of creates a weird timing issue when the request closes before the finacks are sent and received
        req->procedure = clean_up;
        req->paused = true;
    }
}


void on_server_time_out(Response res, Request *req) {
    

    if (req->paused || req->transmission_mode == UN_ACKNOWLEDGED_MODE)
        return;

    if (req->local_entity.transaction_finished_indication == true && RESEND_FINISHED_TIMES != req->resent_finished){
        resend_finished_pdu(req, res);
        return;
    }
    if (req->resent_finished == RESEND_FINISHED_TIMES && req->local_entity.transaction_finished_indication) {
        req->procedure = clean_up;
        ssp_printf("file sent, closing request transaction: %llu\n", req->transaction_sequence_number);
        return;
    }
    //send request for metadata
    if (!req->local_entity.Metadata_recv_indication) {
        request_metadata(req, res);
        return;
    }

    //receiving just messages, send back finished
    if (req->file_size == 0 && RESEND_FINISHED_TIMES != req->resent_finished) {
        resend_finished_pdu(req, res);
        return;
    }

    //send missing eofs
    if (!req->local_entity.EOF_recv_indication) {
        request_eof(req, res);
    }

    //received EOF, send back 3 eof ack packets
    else if (req->local_entity.EOF_recv_indication && req->resent_eof < RESEND_EOF_TIMES) {
        resend_eof_ack(req, res);
    }

    
    //if have not received metadata for a file tranaction, this should not ever trigger //TODO add asert
    if (req->file == NULL) {
        ssp_printf("file is null, not sending data naks");
        return;
    }
    //send missing NAKS
    if (req->file->missing_offsets->count > 0) {
        request_data(req, res);
        return;

    } else {
        //finished transaction, should have checksum complete, and received eof notification
        if (req->file->eof_checksum == req->file->partial_checksum && req->local_entity.EOF_recv_indication) {
            ssp_printf("checksum have: %u checksum_need: %u\n", req->file->partial_checksum, req->file->eof_checksum);
            req->local_entity.transaction_finished_indication = true;
            resend_finished_pdu(req, res);
            return;
        }
        
        //ssp_printf("checksum have: %u checksum_need: %u\n", req->file->partial_checksum, req->file->eof_checksum);
        //uint32_t checksum = check_sum_file(req->file, 1000);
        //ssp_printf("checksum re-calculated: %u\n", checksum);
        
    }

}

//fills the current_request struct for the server, incomming requests
int parse_packet_server(char *packet, uint32_t packet_index, Response res, Request *req, Pdu_header incoming_header, FTP *app) {

    if (packet_index == 0)
        return -1;
        
    uint16_t data_len = get_data_length(packet);
    uint32_t packet_len = packet_index + data_len;
    int error = 0;

    //process file data
    if (incoming_header.PDU_type == DATA) {
        if (!req->local_entity.Metadata_recv_indication) {
            if (req->file == NULL) {
                transasction_log("file is null", incoming_header.transaction_sequence_number);
                error = get_req_from_file(incoming_header.source_id, incoming_header.transaction_sequence_number, incoming_header.destination_id, req);
                if (error < 0)
                    build_temperary_file(req, TEMP_FILESIZE);
                
            }
        }
        process_data_packet(&packet[packet_index], data_len, req->file);
        return packet_len;
    }
    

    Pdu_directive *directive = (Pdu_directive *) &packet[packet_index];
    Pdu_ack ack_packet;
    packet_index++;

    switch (directive->directive_code)
    {
        case META_DATA_PDU:
            if (req->local_entity.Metadata_recv_indication)
                break;

            transasction_log("received metadata packet transaction", incoming_header.transaction_sequence_number);
            process_metadata(packet, packet_index, res, req, app);
            break;
    
        case EOF_PDU:
            if (req->local_entity.EOF_recv_indication)
                break;

            transasction_log("received eof packet", incoming_header.transaction_sequence_number);
            process_pdu_eof(&packet[packet_index], req, res);
            break;

        case ACK_PDU:
            
            get_ack_from_packet(&packet[packet_index], &ack_packet);

            switch (ack_packet.directive_code)
            {
                case FINISHED_PDU:
                    //get_finished_pdu(char *packet, Pdu_finished *pdu_finished)
                    transasction_log("received finished packet Ack", incoming_header.transaction_sequence_number);
                    req->local_entity.transaction_finished_indication = true;
                    break;
            
                default:
                    break;
            }
            break;
        default:
            break;
    }

    return packet_len;
}

