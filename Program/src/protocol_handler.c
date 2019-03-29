

#include "port.h"
#include "protocol_handler.h"
#include "string.h"
#include "packet.h"





//returns the location in the packet where the next pointer for tthe packet will start after the header
static uint8_t build_pdu_header(Response res, Request *req, Client* client, Protocol_state *p_state) {
    unsigned char *packet = res.msg;
    memcpy(packet, client->pdu_header, PACKET_STATIC_HEADER_LEN);

    //copy variable length src id
    memcpy(&packet[PACKET_STATIC_HEADER_LEN], 
    client->pdu_header->source_id, 
    client->pdu_header->length_of_entity_IDs);
    
    //copy variable length transaction number id
    memcpy(&packet[PACKET_STATIC_HEADER_LEN + client->pdu_header->length_of_entity_IDs],
    &req->transaction_sequence_number, 
    client->pdu_header->transaction_seq_num_len);

    //copy variable length destination id
    memcpy(&packet[PACKET_STATIC_HEADER_LEN + client->pdu_header->length_of_entity_IDs + client->pdu_header->transaction_seq_num_len],
    client->pdu_header->destination_id,
    client->pdu_header->length_of_entity_IDs);
    
    uint8_t total_bytes = PACKET_STATIC_HEADER_LEN 
    + client->pdu_header->length_of_entity_IDs 
    + client->pdu_header->transaction_seq_num_len 
    + client->pdu_header->length_of_entity_IDs;

    return total_bytes;
}

static uint8_t build_put_packet_metadata(Response res, uint32_t start, Request *req, Client* client, Protocol_state *p_state) {    
    Pdu_header *header = (Pdu_header *) res.msg;
   
    //set header to file directive 0 is directive, 1 is data
    header->PDU_type = 0;
    
    uint8_t packet_index = start;

    //set directive 1 byte
    Pdu_directive *directive = &res.msg[packet_index];
    directive->directive_code = META_DATA_PDU;
    packet_index += SIZE_OF_DIRECTIVE_CODE;
    Pdu_meta_data *meta_data_packet = &res.msg[packet_index];

    //1 bytes
    meta_data_packet->segmentation_control = req->segmentation_control;
    meta_data_packet->reserved_bits = 0;
    
    //4 bytes
    meta_data_packet->file_size = req->file_size;
    packet_index += 5;

    //variable length params
    uint8_t src_file_name_length = strnlen(req->source_file_name, MAX_PATH);
    uint8_t destination_file_length = strnlen(req->destination_file_name, MAX_PATH);
    char *src_file_name = req->source_file_name;
    char *destination_file_name = req->destination_file_name;
    
    
    //copy source length to packet (1 bytes) 
    memcpy(&res.msg[packet_index], &src_file_name_length, src_file_name_length);
    packet_index++;
    //copy source name to packet (length bytes) 
    memcpy(&res.msg[packet_index], src_file_name, src_file_name_length);
    packet_index += src_file_name_length;


    //copy length to packet (1 byte)
    memcpy(&res.msg[packet_index], &destination_file_length, 1);
    packet_index++;
    
    //copy destination name to packet (length bytes)
    memcpy(&res.msg[packet_index], destination_file_name, destination_file_length);
    packet_index += destination_file_length;

    uint8_t total_bytes = packet_index - start; 

    //mark the size of the packet
    header->PDU_data_field_len = total_bytes;
    return packet_index;
}

//requires a req->file to be created
//returns 1 on end of file
static uint8_t build_data_packet(Response res, uint32_t start, Request *req, Client* client, Protocol_state *p_state) {

    if (req->file->next_offset_to_send > req->file->total_size){
        ssp_error("cant send an offset past the file's length\n");
        return -1;
    }

    Pdu_header *header = (Pdu_header *) res.msg;
    //set header to file directive 0 is directive, 1 is data
    header->PDU_type = 1;

    uint16_t packet_index = start;
    File_data_pdu_contents *packet_offset = &res.msg[packet_index];
    
    //4 bytes is the size of the offset paramater TODO set offset
    packet_offset->offset = req->file->next_offset_to_send;
    packet_index += 4;

    uint16_t data_size = client->packet_len - packet_index;
    
    //fill the rest of the packet with data
    int bytes = get_offset(req->file, &res.msg[packet_index], data_size, packet_offset->offset);
    req->file->next_offset_to_send += bytes;

    //add bytes read, and the packet offset to the data_field length
    header->PDU_data_field_len = bytes + 4;

    if (bytes <  data_size)
        return 1;

    return 0;
}

static void build_eof_packet(Response res, uint32_t start, Request *req, Client* client, Protocol_state *p_state) {

    Pdu_header *header = (Pdu_header *) res.msg;
    //set header to file directive 0 is directive, 1 is data
    header->PDU_type = 0;
    
    uint8_t packet_index = (uint8_t) start;
    Pdu_directive *directive = &res.msg[packet_index];
    directive->directive_code = EOF_PDU;
    packet_index++;

    Pdu_eof *packet = &res.msg[packet_index];

    //this will be need to set from the req struct probably.
    //4 bits, 
    packet->condition_code = COND_NO_ERROR;
    //4 bits reserved bits
    packet->spare = 0;
    packet_index++;

    //4 bytes
    packet->file_size = req->file_size;
    packet_index += 4;

    //TODO checksum procedures
    packet->checksum = 0;
    packet_index += 4;


    //TODO addTLV fault_location
    header->PDU_data_field_len = packet_index - start;

}


//TODO This needs more work, file handling when files already exist ect
static int process_file_request_metadata(Request *req) {

    if (does_file_exist(req->destination_file_name)){
        ssp_error("file already exists, overwriting it\n");
        req->file = create_file(req->destination_file_name);
        return 1;
    }
    if (req->file == NULL) {
        ssp_printf("%s\n", req->destination_file_name);
        req->file = create_file(req->destination_file_name);
    }
    return 1;
}


static void write_packet_data_to_file(char *data_packet, uint32_t data_len,  File *file) {
    if(file == NULL)
        ssp_error("file struct is null, can't write to file");

    File_data_pdu_contents *packet = data_packet;
    uint32_t offset = packet->offset;

    //ssp_printf("packet offset received: %d\n", packet->offset);
    int bytes = write_offset(file, &data_packet[4], data_len - 4, offset);
}


static void fill_request_pdu_metadata(unsigned char *meta_data_packet, Request *req_to_fill) {

    Pdu_meta_data *meta_data = meta_data_packet;
    req_to_fill->segmentation_control = meta_data->segmentation_control;

    uint8_t packet_index = 4;

    uint32_t file_size = (uint32_t)meta_data_packet[packet_index];
    req_to_fill->file_size = file_size;
    packet_index++;

    uint8_t file_name_len = meta_data_packet[packet_index];
    packet_index++;


    memcpy(req_to_fill->source_file_name, &meta_data_packet[packet_index], file_name_len);

    packet_index += file_name_len + 1;
    file_name_len = meta_data_packet[packet_index];
    memcpy(req_to_fill->destination_file_name, &meta_data_packet[packet_index], file_name_len);

    packet_index += file_name_len;

    return;
}


/*------------------------------------------------------------------------------

                                    REMOTE SIDE
                                    aka: handles responses from server

------------------------------------------------------------------------------*/



//fills the current request with packet data, responses from servers
void parse_packet_client(unsigned char *msg, Request *current_request, Client* client, Protocol_state *p_state) {
    ssp_printf("client received %x\n", msg);
}


//Client responses
void packet_handler_client(Response res, Request *req, Client* client, Protocol_state *p_state) {
    //res.msg = "Client Received!\n";
    //ssp_sendto(res);
}  


/*------------------------------------------------------------------------------

                                    SERVER SIDE
                                    aka: handles responses from remote

------------------------------------------------------------------------------*/

//Server responses
void packet_handler_server(Response res, Request *current_request, Protocol_state *p_state) {
    //res.msg = "Server Received\n";
    //ssp_sendto(res);
}


//fills the current_request struct for the server, incomming requests
void parse_packet_server(unsigned char *packet, uint32_t packet_len, Request *current_request, Protocol_state *p_state) {

    uint8_t packet_index = PACKET_STATIC_HEADER_LEN;
    Pdu_header *header = (Pdu_header *) packet;

    uint32_t source_id = 0;
    memcpy(&source_id, &packet[packet_index], header->length_of_entity_IDs);
    packet_index += header->length_of_entity_IDs;

    //TODO the transaction number should get the request from data structure hosting requests
    uint32_t transaction_sequence_number = 0;
    memcpy(&transaction_sequence_number, &packet[packet_index], header->transaction_seq_num_len);
    packet_index += header->transaction_seq_num_len;

    uint32_t dest_id = 0;
    memcpy(&dest_id, &packet[packet_index], header->length_of_entity_IDs);
    packet_index += header->length_of_entity_IDs;

    uint16_t packet_data_len = header->PDU_data_field_len;


    if (p_state->verbose_level == 3) {
        ssp_printf("------------printing_header_received------------\n");
        ssp_print_hex(packet, packet_index);
        ssp_printf("packet data length %d, sequence number %d\n", packet_data_len, transaction_sequence_number);
    }

    if (p_state->my_cfdp_id != dest_id){
        ssp_printf("someone is sending packets here that are not for me\n");
        return;
    }
    //process file data
    if (header->PDU_type == 1) {
        write_packet_data_to_file(&packet[packet_index], packet_data_len, current_request->file);
        return;
    }

    current_request->dest_cfdp_id = source_id;
    Pdu_directive *directive = &packet[packet_index];
    packet_index++;

    switch (directive->directive_code)
    {
        case META_DATA_PDU:
            fill_request_pdu_metadata(&packet[packet_index], current_request);
            process_file_request_metadata(current_request);
            break;
    
        case EOF_PDU:

            ssp_printf("received EOF pdu\n");
            
            break;
        default:
            break;
    }

    memset(packet, 0, packet_len);
}




/*------------------------------------------------------------------------------

                                    USER STUFF
                                    aka: request from person

------------------------------------------------------------------------------*/


//current user request, to send to remote
void user_request_handler(Response res, Request *req, Client* client, Protocol_state *p_state) {

    if (req->type == none)
        return;

    res.msg = req->buff;
    memset(res.msg, 0, client->packet_len);

    uint32_t start = build_pdu_header(res, req, client, p_state);

    switch (req->type)
    {
        case eof: 
            build_eof_packet(res, start, req, client, p_state);
            ssp_sendto(res);
            req->type = none;
            break;

        case sending_data: 
            if (build_data_packet(res, start, req, client, p_state))
                req->type = eof;
            
            if (p_state->verbose_level == 3) {
                ssp_printf("------------sending_a_data_packets-----------\n");
                ssp_print_hex(res.msg, start);
            }
            ssp_sendto(res);
            break;

        case put:
            start = build_put_packet_metadata(res, start, req, client, p_state);
            
            if (p_state->verbose_level == 3) {
                ssp_printf("------------sending_a_put_request------------\n");
                ssp_print_hex(res.msg, start);
            }

            ssp_sendto(res);
            req->type = sending_data;
            break;


        default:
            break;
    }

}







//Omission of source and destination filenames shall indicate that only Meta
//data will be delivered
int put_request(unsigned char *source_file_name,
            unsigned char *destination_file_name,
            uint8_t segmentation_control,
            uint8_t fault_handler_overides,
            uint8_t flow_lable,
            uint8_t transmission_mode,
            unsigned char* messages_to_user,
            unsigned char* filestore_requests,
            Client *client,
            Protocol_state *p_state
            ) {

    uint32_t file_size = get_file_size(source_file_name);
    
    if (file_size == -1)
        return -1;

    //give the client a new request to perform
    Request *req = client->outGoing_req;
    req->file = create_file(source_file_name);
    //build a request 
    req->transaction_sequence_number = p_state->transaction_id++;
    //enumeration
    req->type = put;
    req->dest_cfdp_id = client->cfdp_id;
    req->file_size = file_size;
    
    memcpy(req->source_file_name, source_file_name ,strnlen(source_file_name, MAX_PATH));
    memcpy(req->destination_file_name, destination_file_name, strnlen(destination_file_name, MAX_PATH));

    req->segmentation_control = segmentation_control;
    req->fault_handler_overides = fault_handler_overides;
    req->flow_lable = flow_lable;
    req->transmission_mode = transmission_mode;
    req->messages_to_user = messages_to_user;
    req->filestore_requests = filestore_requests;
}


