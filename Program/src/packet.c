/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#include "types.h"
#include "packet.h"
#include "utils.h"
#include "port.h"
#include "filesystem_funcs.h"
#include "requests.h"

/*------------------------------------------------------------------------------

                                    creating packets

------------------------------------------------------------------------------*/


// if is_data_packet is false, then is directive pacnket
static void set_packet_header(char *packet, uint16_t data_len, bool is_data_packet) {
    set_bits_to_protocol_byte(&packet[0], 3,3, is_data_packet);
    set_data_length(packet, data_len);
}

void set_bits_to_protocol_byte(char *byte, uint8_t from_position, uint8_t to_position, uint8_t value) {
    char bit_mask = value;
    uint8_t bits_to_shift_left = 7-to_position;
    char bits_to_add = bit_mask << bits_to_shift_left;
    *byte = *byte | bits_to_add;
}
//get bits fromleft to right
uint8_t get_bits_from_protocol_byte(char byte, uint8_t from_position, uint8_t to_position){
    uint8_t bits_to_shift_left = from_position;
    uint8_t bits_to_shift_right = 8 - (to_position - from_position + 1);
    uint8_t bits_to_get = byte << bits_to_shift_left;
    uint32_t value = bits_to_get >> bits_to_shift_right;
    return value;
}

void set_packet_directive(char *packet, uint32_t location, uint8_t directive){
    packet[location] = directive;
}

//returns length of ids or -1 on error
int copy_id_to_packet(char *bytes, uint64_t id, uint32_t length_of_ids) {
   
    if (length_of_ids == 8) {
        uint64_t network_byte_order = ssp_htonll(id);
        memcpy(bytes, &network_byte_order, sizeof(uint64_t));   
    } else if (length_of_ids == 4) {
        uint32_t network_byte_order = ssp_htonl((uint32_t) id);
        memcpy(bytes, &network_byte_order, sizeof(uint32_t));   
    } else if (length_of_ids == 2) {
        uint16_t network_byte_order = ssp_htons((uint16_t) id);
        memcpy(bytes, &network_byte_order, sizeof(uint16_t));   
    } else if (length_of_ids == 1) {
        uint8_t network_byte_order = id;
        memcpy(bytes, &network_byte_order, sizeof(uint8_t));   
    } else {
        ssp_error("id size is not supported, please user 1, 2 or 4");
        return -1;
    }
    return length_of_ids;
}

//returns id or -1 on error
uint64_t copy_id_from_packet(char *bytes, uint32_t length_of_ids) {
    uint64_t host_byte_order = 0;
    if (length_of_ids == 8) {
        host_byte_order = ssp_ntohll(*(uint64_t*) bytes);  
    }else if (length_of_ids == 4) {
        host_byte_order = ssp_ntohl(*(uint32_t*) bytes);
    } else if (length_of_ids == 2) {
        host_byte_order = ssp_ntohs(*(uint16_t*) bytes); 
    } else if (length_of_ids == 1){
        host_byte_order = *bytes;
    } else {
        ssp_error("id size is not supported, please user 1, 2 or 4");
        return -1;
    }
    return host_byte_order;
}

//returns amount of bytes written or -1 on error
int copy_id_lv_to_packet(char *bytes, LV id) {

    memcpy(bytes, &id.length, sizeof(uint8_t));
    int len = copy_id_to_packet(&bytes[1], *(uint64_t*)id.value, id.length);
    if (len < 0) {
        return -1;
    }
    return len + 1;
}

//returns 0 on success and sets LV memory, or -1 on error
int copy_id_lv_from_packet(char *bytes, LV *lv){

    uint8_t len = 0;
    memcpy(&len, bytes, sizeof(uint8_t));

    uint64_t id = copy_id_from_packet(&bytes[1], len);
    if (id < 0) {
        ssp_printf("failed to copy id from packet %d\n", id);
        return -1;
    }
    create_lv(lv, len, &id);
    return 0;
}


void ssp_print_header(Pdu_header *pdu_header){

    ssp_printf("pdu_header->version %d\n",pdu_header->version);
    ssp_printf("pdu_header->PDU_type %d\n",pdu_header->PDU_type);
    ssp_printf("pdu_header->direction %d\n",pdu_header->direction);
    ssp_printf("pdu_header->transmission_mode %d\n",pdu_header->transmission_mode);
    ssp_printf("pdu_header->CRC_flag %d\n",pdu_header->CRC_flag);
    ssp_printf("pdu_header->reserved_bit_0 %d\n",pdu_header->reserved_bit_0);
    ssp_printf("pdu_header->PDU_data_field_len %d\n",pdu_header->PDU_data_field_len);
    ssp_printf("pdu_header->reserved_bit_1 %d\n",pdu_header->reserved_bit_1 );
    ssp_printf("pdu_header->length_of_entity_IDs %d\n",pdu_header->length_of_entity_IDs);
    ssp_printf("pdu_header->reserved_bit_2 %d\n",pdu_header->reserved_bit_2);
    ssp_printf("pdu_header->transaction_seq_num_len %d\n",pdu_header->transaction_seq_num_len);
    ssp_printf("pdu_header->source_id %d\n",pdu_header->source_id);
    ssp_printf("pdu_header->transaction_sequence_number %d\n",pdu_header->transaction_sequence_number);
    ssp_printf("pdu_header->destination_id %d\n",pdu_header->destination_id);


}

int get_pdu_header_from_packet(char *packet, Pdu_header *pdu_header){

    pdu_header->version = get_bits_from_protocol_byte(packet[0], 0, 2);
    pdu_header->PDU_type = get_bits_from_protocol_byte(packet[0], 3, 3);
    pdu_header->direction = get_bits_from_protocol_byte(packet[0], 4, 4);
    pdu_header->transmission_mode = get_bits_from_protocol_byte(packet[0], 5, 5);
    pdu_header->CRC_flag = get_bits_from_protocol_byte(packet[0], 6, 6);
    pdu_header->reserved_bit_0 = get_bits_from_protocol_byte(packet[0], 7, 7);

    pdu_header->PDU_data_field_len = get_data_length(packet);

    pdu_header->reserved_bit_1 = get_bits_from_protocol_byte(packet[3], 0, 0);
    pdu_header->length_of_entity_IDs = get_bits_from_protocol_byte(packet[3], 1, 3);
    pdu_header->reserved_bit_2 = get_bits_from_protocol_byte(packet[3], 4, 4);
    pdu_header->transaction_seq_num_len = get_bits_from_protocol_byte(packet[3], 5, 7);
    

    int32_t source_id_location = PACKET_STATIC_HEADER_LEN;
    pdu_header->source_id = copy_id_from_packet(&packet[source_id_location], pdu_header->length_of_entity_IDs);
    if (pdu_header->source_id < 0) {
        ssp_error("failed to get source_id");
        return -1;
    }   

    int32_t transaction_number_location = source_id_location + pdu_header->length_of_entity_IDs;
    pdu_header->transaction_sequence_number = copy_id_from_packet(&packet[transaction_number_location], pdu_header->transaction_seq_num_len);
    if (pdu_header->transaction_sequence_number < 0) {
        ssp_error("failed to get transaction_sequence_number");
        return -1;
    }   

    int32_t dest_id_location = transaction_number_location + pdu_header->transaction_seq_num_len;
    pdu_header->destination_id = copy_id_from_packet(&packet[dest_id_location], pdu_header->length_of_entity_IDs);
    if (pdu_header->destination_id < 0) {
        ssp_error("failed to get destination_id");
        return -1;
    }   
    pdu_header->reserved_space_for_header = dest_id_location + pdu_header->length_of_entity_IDs;
    return pdu_header->reserved_space_for_header;
}

//returns the location in the packet where the next pointer for tthe packet will start after the header
int build_pdu_header(char *packet, uint64_t transaction_sequence_number, uint32_t transmission_mode, uint16_t data_len, Pdu_header *pdu_header) {
    memset(packet, 0, 4);

    set_bits_to_protocol_byte(&packet[0], 0,2, pdu_header->version);
    set_bits_to_protocol_byte(&packet[0], 3,3, pdu_header->PDU_type);
    set_bits_to_protocol_byte(&packet[0], 4,4, pdu_header->direction);
    set_bits_to_protocol_byte(&packet[0], 5,5, transmission_mode);
    set_bits_to_protocol_byte(&packet[0], 6,6, pdu_header->CRC_flag);
    set_bits_to_protocol_byte(&packet[0], 7,7, pdu_header->reserved_bit_0);
    set_data_length(packet, data_len);
    set_bits_to_protocol_byte(&packet[3], 0,0, pdu_header->reserved_bit_1);
    set_bits_to_protocol_byte(&packet[3], 1,3, pdu_header->length_of_entity_IDs);
    set_bits_to_protocol_byte(&packet[3], 4,4, pdu_header->reserved_bit_2);
    set_bits_to_protocol_byte(&packet[3], 5,7, pdu_header->transaction_seq_num_len);

    int32_t source_id_location = PACKET_STATIC_HEADER_LEN;
    int error = copy_id_to_packet(&packet[source_id_location], pdu_header->source_id, pdu_header->length_of_entity_IDs);
    if (error < 0) {
        ssp_error("failed copy source_id");
        return -1;
    }   

    int32_t transaction_number_location = source_id_location + pdu_header->length_of_entity_IDs;
    error = copy_id_to_packet(&packet[transaction_number_location], (uint32_t) transaction_sequence_number, pdu_header->transaction_seq_num_len);
    if (error < 0) {
        ssp_error("failed copy transaction_number_location");
        return -1;
    }   
    int32_t dest_id_location = transaction_number_location + pdu_header->transaction_seq_num_len;
    error = copy_id_to_packet(&packet[dest_id_location], pdu_header->destination_id, pdu_header->length_of_entity_IDs);
    if (error < 0) {
        ssp_error("failed copy destination_id");
        return -1;
    }   

    return dest_id_location + pdu_header->length_of_entity_IDs;
}

uint8_t build_finished_pdu(char *packet, uint32_t start) {

    uint32_t packet_index = start;
    uint16_t data_len = 0;

    packet[packet_index] = FINISHED_PDU;
    packet_index++;
    data_len++;

    //one byte
    Pdu_finished *pdu_finished = (Pdu_finished *) &packet[packet_index];
    pdu_finished->condition_code = COND_NO_ERROR;
    pdu_finished->delivery_code = 0;
    pdu_finished->file_status = FILE_RETAINED_SUCCESSFULLY;
    data_len += 1;
    packet_index += 1;

    set_packet_header(packet, data_len, DIRECTIVE);
    return data_len;
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

//returns packet_index for data, to get length of meta data, subtract start from return value
uint8_t build_put_packet_metadata(char *packet, uint32_t start, Request *req) {    

    uint8_t packet_index = start;

    //set directive 1 byte
    set_packet_directive(packet, packet_index, META_DATA_PDU);
    packet_index += SIZE_OF_DIRECTIVE_CODE;

    //Set segmentation_control bit and 7 reserved bits (to 0)
    set_bits_to_protocol_byte(&packet[packet_index], 0, 0, req->segmentation_control);
    packet_index++;

    //4 bytes
    uint32_t network_bytes = ssp_htonl(req->file_size);
    memcpy(&packet[packet_index], &network_bytes, sizeof(uint32_t));
    packet_index += 4;

    //variable length params
    uint8_t src_file_name_length = strnlen(req->source_file_name, MAX_PATH);
    uint8_t destination_file_length = strnlen(req->destination_file_name, MAX_PATH);

    //copy source length to packet (1 bytes) 
    memcpy(&packet[packet_index], &src_file_name_length, 1);
    packet_index++;
    //copy source name to packet (length bytes) 
    memcpy(&packet[packet_index], req->source_file_name, src_file_name_length);
    packet_index += src_file_name_length;
    //copy length to packet (1 byte)
    memcpy(&packet[packet_index], &destination_file_length, 1);
    packet_index++;
    //copy destination name to packet (length bytes)
    memcpy(&packet[packet_index], req->destination_file_name, destination_file_length);
    packet_index += destination_file_length;

    //add messages to metadata
    packet_index += add_messages_to_packet(packet, packet_index, req->messages_to_user);

    uint8_t data_len = packet_index - start; 
    set_packet_header(packet, data_len, DIRECTIVE);
    
    return packet_index;
}
uint8_t build_nak_response(char *packet, uint32_t start, uint32_t offset, Request *req, Client* client) {

    if (offset > req->file->total_size) {
        return 1;
    }

    uint16_t packet_index = start;
    File_data_pdu_contents *packet_offset = (File_data_pdu_contents *) &packet[packet_index];
    packet_offset->offset = offset;

    //4 bytes is the size of the offset paramater
    packet_index += 4;
    uint16_t data_size = client->packet_len - packet_index;
    //fill the rest of the packet with data
    int bytes = get_offset(req->file, &packet[packet_index], data_size, offset);
    if (bytes <= 0){
        ssp_error("could not get offset, this could because the file is empty!\n");
        return 1;
    }
    
    uint16_t data_len = bytes + 4;
    set_packet_header(packet, data_len, DATA);

    if (bytes <  data_size)
        return 1;

    return 0;
}



/*
typedef struct file_data_pdu_contents {
    //in octets
    uint32_t offset;
    //variable sized
    unsigned char *data;
} File_data_pdu_contents;
*/

//returns the offset in the file (for the data packet)
uint32_t get_data_offset_from_packet(char *packet) {
    uint32_t offset = 0;
    memcpy(&offset, packet, sizeof(uint32_t));

    offset = ssp_ntohl(offset);
    return offset;
}

int build_data_packet(char *packet, uint32_t start, File *file, uint32_t length) {

    //don't accidently send more than file size, this can probably be outside this packet
    if (file->next_offset_to_send >= file->total_size){
        return 0;
    }

    uint16_t packet_index = start;

    uint32_t offset = 0;
    offset = ssp_htonl(file->next_offset_to_send);
    
    memcpy(&packet[packet_index], &offset, sizeof(uint32_t));
    
    packet_index += 4;
    
    uint16_t data_size = length - packet_index;
    
    //fill the rest of the packet with data
    int bytes = get_offset(file, &packet[packet_index], data_size, file->next_offset_to_send);
    if (bytes <= 0){
        ssp_error("could not get offset, this could because the file is empty!\n");
        return -1;
    }

    //calculate checksum for data packet, this is used to calculate in transit checksums
    file->partial_checksum += calc_check_sum(&packet[packet_index], bytes);
    file->next_offset_to_send += bytes;

    //add bytes read, and the packet offset to the data_field length
    uint16_t data_len = bytes + 4;
    set_packet_header(packet, data_len, DATA);

    if (bytes <  data_size)
        return 1;

    return 0;
}

/*
typedef struct pdu_eof {
    unsigned int condition_code : 4;
    unsigned int spare : 4;
    uint32_t checksum;

    //In octets. This value shall be the total number of file data octets
    //transmitted by the sender, regardless of the condition code
    //(i.e., it shall be supplied even if the condition code is other than
    //‘No error’).
    uint32_t file_size;

    
    //Omitted if condition code is ‘No error’. Otherwise, entity ID in the
    //TLV is the ID of the entity at which transaction cancellation was
    //initiated.
    TLV fault_location;
    
} Pdu_eof;
*/
void get_eof_from_packet(char *packet, Pdu_eof *eof){
    uint32_t packet_index = 0;
    uint32_t filesize = 0;
    uint32_t checksum = 0;

    eof->condition_code = get_bits_from_protocol_byte(packet[packet_index], 0, 3);
    eof->spare = get_bits_from_protocol_byte(packet[packet_index], 4, 7);

    packet_index++;
    memcpy(&filesize, &packet[packet_index], sizeof(uint32_t)); 
    eof->file_size = ssp_ntohl(filesize);

    packet_index += sizeof(uint32_t);
    memcpy(&checksum, &packet[packet_index], sizeof(uint32_t));
    eof->checksum = ssp_ntohl(checksum);

    //todo add fault location parsing
}

void build_eof_packet(char *packet, uint32_t start, uint32_t file_size, uint32_t checksum) {

    uint8_t packet_index = start;

    //set directive 1 byte
    set_packet_directive(packet, packet_index, EOF_PDU);
    packet_index += SIZE_OF_DIRECTIVE_CODE;

    //this will be need to set from the req struct probably.
    set_bits_to_protocol_byte(&packet[packet_index], 0, 3, COND_NO_ERROR);

    //4 bits reserved bits
    set_bits_to_protocol_byte(&packet[packet_index], 4, 7, 0);

    packet_index++;

    //4 bytes
    uint32_t network_file_size = ssp_htonl(file_size);
    memcpy(&packet[packet_index], &network_file_size, sizeof(uint32_t));
    packet_index += sizeof(uint32_t);
    
    //4 bytes
    uint32_t network_checksum = ssp_htonl(checksum);
    memcpy(&packet[packet_index], &network_checksum, sizeof(uint32_t));
    packet_index += sizeof(uint32_t);

    //TODO addTLV fault_location

    uint16_t data_len = packet_index - start;
    set_packet_header(packet, data_len, DIRECTIVE);
}

//this is a callback for building nak_array server side
struct packet_nak_helper {
    char*packet;
    uint64_t max_number_of_nak_segments;
    uint64_t current_number_of_segments;
    uint32_t start_scope;
    uint32_t end_scope;
};

void fill_nak_array_callback(Node *node, void *element, void *args){
    struct packet_nak_helper *holder = (struct packet_nak_helper *)args;

    if (holder->current_number_of_segments == holder->max_number_of_nak_segments)
        return;

    Offset *offset = (Offset *)element;
    Offset offset_to_copy;
    offset_to_copy.start = ssp_htonl(offset->start);
    offset_to_copy.end = ssp_htonl(offset->end);

    memcpy(holder->packet, &offset_to_copy, sizeof(Offset));

    holder->current_number_of_segments++;
    holder->packet+=sizeof(Offset);
}

uint32_t build_nak_packet(char *packet, uint32_t start, Request *req) {
    
    packet[start] = NAK_PDU;
    uint32_t packet_index = start + 1;
    Pdu_nak *pdu_nak = (Pdu_nak *) &packet[packet_index];
    packet_index += 16;
    
    struct packet_nak_helper holder;

    holder.max_number_of_nak_segments = (req->buff_len - packet_index) / sizeof(Offset);
   
    holder.packet = &packet[packet_index];
    holder.current_number_of_segments = 0;
    holder.start_scope = 0;
    holder.end_scope = 0;

    req->file->missing_offsets->iterate(req->file->missing_offsets, fill_nak_array_callback, &holder);

    uint32_t start_scope = ((Offset *)req->file->missing_offsets->head->next->element)->start;
    uint32_t end_scope = ((Offset *)req->file->missing_offsets->tail->prev->element)->end;

    pdu_nak->start_scope = ssp_htonl(start_scope);
    pdu_nak->end_scope = ssp_htonl(end_scope);
    pdu_nak->segment_requests = ssp_htonll(holder.current_number_of_segments);

    packet_index += sizeof(Offset) * holder.current_number_of_segments;

    uint16_t data_len = packet_index - start;
    set_packet_header(packet, data_len, DIRECTIVE);

    return data_len;
}

/*

typedef struct pdu_ack {
    //Directive code of the acknowledged PDU.
    unsigned int directive_code : 4;

    //Values depend on directive code. For ACK of Finished PDU: binary ‘0000’
    //if Finished PDU is generated by waypoint, binary ‘0001’ if Finished
    //PDU is generated by end system. (NOTE: this discrimination is
    //meaningful if the Extended Procedures are implemented.)
    //Binary ‘0000’ for ACKs of all other file directives.
    
    unsigned int directive_subtype_code : 4;

    //Condition code of the acknowledged PDU.
    unsigned int condition_code : 4;
    unsigned int spare : 2;

    //Status of the transaction in the context of the entity that is issuing the acknowledgment.
    unsigned int transaction_status : 2;

} Pdu_ack;
*/

//this function should be called at the buffer location after an ACK_PDU bits were set
void get_ack_from_packet(char *packet, Pdu_ack *ack){
    ack->directive_code = get_bits_from_protocol_byte(packet[0], 0, 3);
    ack->directive_subtype_code = get_bits_from_protocol_byte(packet[0], 4, 7);
    ack->condition_code = get_bits_from_protocol_byte(packet[1], 0,  3);
    ack->spare = get_bits_from_protocol_byte(packet[1], 4, 5);
    ack->transaction_status = get_bits_from_protocol_byte(packet[1], 6, 7);
}

uint8_t build_ack(char*packet, uint32_t start, uint8_t type) {
    //first byte indicates that its a ACK_PDU
    packet[start] = ACK_PDU;
    uint32_t packet_index = start + 1;

    set_bits_to_protocol_byte(&packet[packet_index], 0, 3, type);
    set_bits_to_protocol_byte(&packet[packet_index], 4, 7, ACK_FINISHED_END);    
    
    packet_index += 1;
    //will need more/propper conditions
    set_bits_to_protocol_byte(&packet[packet_index], 0, 3, COND_NO_ERROR);    

    packet_index += 1;
    uint16_t data_len = packet_index - start;
    set_packet_header(packet, data_len, DIRECTIVE);
    return data_len;
}

uint8_t build_nak_directive(char *packet, uint32_t start, uint8_t directive) {
    uint8_t data_len = 2;
    packet[start] = NAK_DIRECTIVE;
    packet[start + 1] = directive;
    
    set_packet_header(packet, data_len, DIRECTIVE);
    return data_len;
}

void set_data_length(char*packet, uint16_t data_len){
    uint16_t bytes = ssp_htons(data_len); 
    memcpy(&packet[1], &bytes, sizeof(uint16_t));
}

uint16_t get_data_length(char*packet) {
    uint16_t bytes = 0;
    memcpy(&bytes, &packet[1], sizeof(uint16_t));
    uint16_t len = ssp_ntohs(bytes);
    return len;
}

struct packet_callback_params {
    char *packet;
    uint32_t *packet_index;
};

static void add_messages_callback(Node *node, void *element, void *args) {
    struct packet_callback_params *params = (struct packet_callback_params *) args; 
    char *packet = params->packet;
    uint32_t packet_index = *(params->packet_index);
    Message *message = (Message *) element;
    int error, bytes = 0;

    //since this is a callback functions, we can't return -1, intead we just log
    char *error_msg = "there was an issue copying bytes for the message: %s\n";

    //5 bytes to copy cfdp\0 into the buffer
    memcpy(&packet[packet_index], message->header.message_id_cfdp, 5);
    packet_index += 5;

    //one byte for message type
    memcpy(&packet[packet_index], &message->header.message_type, 1);
    packet_index += 1;

    Message_put_proxy *proxy_put;
    Message_cont_part_request *proxy_cont_part;
    switch (message->header.message_type)
    {
        case PROXY_PUT_REQUEST:
            proxy_put = (Message_put_proxy *) message->value;
           
            bytes = copy_id_lv_to_packet(&packet[packet_index], proxy_put->destination_id);
            if (bytes < 0) {
                ssp_printf(error_msg, "PROXY_PUT_REQUEST");
                return;
            }
            packet_index += bytes;
            packet_index += copy_lv_to_buffer(&packet[packet_index], proxy_put->source_file_name);
            packet_index += copy_lv_to_buffer(&packet[packet_index], proxy_put->destination_file_name);
            break;

        case CONTINUE_PARTIAL:
            proxy_cont_part = (Message_cont_part_request *) message->value;

            bytes = copy_id_lv_to_packet(&packet[packet_index], proxy_cont_part->destination_id);
            if (bytes < 0) {
                ssp_printf(error_msg, "CONTINUE_PARTIAL");
                return;
            }
            packet_index += bytes;

            bytes = copy_id_lv_to_packet(&packet[packet_index], proxy_cont_part->originator_id);
            if (bytes < 0) {
                ssp_printf(error_msg, "CONTINUE_PARTIAL");
                return;
            }
            packet_index += bytes;

            bytes = copy_id_lv_to_packet(&packet[packet_index], proxy_cont_part->transaction_id);
            if (bytes < 0) {
                ssp_printf(error_msg, "CONTINUE_PARTIAL");
                return;
            }
            packet_index += bytes;
            break;

        default:
            break;
    }

    *(params->packet_index) = packet_index;

}

//returns length of added messages, including the start; copys messages into packet
uint32_t add_messages_to_packet(char *packet, uint32_t start, List *messages_to_user) {
    
    uint32_t packet_index = start;
    struct packet_callback_params params = {packet, &packet_index};
    messages_to_user->iterate(messages_to_user, add_messages_callback, &params);
    return packet_index;
}


//adds messages from packet into request, returns the location of the next message
uint32_t get_message_from_packet(char *packet, uint32_t start, Request *req) {

    if (strncmp(&packet[start], "cfdp", 5) != 0) {
        ssp_error("messages are poorly formatted\n");
        return 0;
    }

    Message *m;
    Message_put_proxy *proxy_put;
    Message_cont_part_request *proxy_cont_part;

    uint32_t message_type = start + 5;
    uint32_t message_start = start + 6;

    int error = 0;
    switch (packet[message_type])
    {
        case PROXY_PUT_REQUEST:
            m = create_message(PROXY_PUT_REQUEST);
            
            m->value = ssp_alloc(1, sizeof(Message_put_proxy));
            if (m->value == NULL) {
                return -1;
            }
            proxy_put = (Message_put_proxy *) m->value;

            error = copy_id_lv_from_packet(&packet[message_start], &proxy_put->destination_id);
            if (error < 0) {
                ssp_free(m->value);
                return -1;
            }

            message_start += proxy_put->destination_id.length + 1;
            
            copy_lv_from_buffer(&proxy_put->source_file_name, packet, message_start);
            message_start += proxy_put->source_file_name.length + 1;

            copy_lv_from_buffer(&proxy_put->destination_file_name, packet, message_start);
            message_start += proxy_put->destination_file_name.length + 1;
            break;

        case CONTINUE_PARTIAL:
            m = create_message(CONTINUE_PARTIAL);
            
            m->value = ssp_alloc(1, sizeof(Message_cont_part_request));
            proxy_cont_part = (Message_cont_part_request *) m->value;
           
            error = copy_id_lv_from_packet(&packet[message_start], &proxy_cont_part->destination_id);
            if (error < 0) {
                ssp_free(m->value);
                return -1;
            }
            message_start += proxy_cont_part->destination_id.length + 1;

            error = copy_id_lv_from_packet(&packet[message_start], &proxy_cont_part->originator_id);
            if (error < 0) {
                ssp_free(m->value);
                return -1;
            }
            message_start += proxy_cont_part->originator_id.length + 1;

            error = copy_id_lv_from_packet(&packet[message_start], &proxy_cont_part->transaction_id);
            if (error < 0) {
                ssp_free(m->value);
                return -1;
            }
            message_start += proxy_cont_part->transaction_id.length + 1;

        default:
            break;
    }

    req->messages_to_user->push(req->messages_to_user, m, 0);
    return message_start;
}


uint32_t get_messages_from_packet(char *packet, uint32_t start, uint32_t data_length, Request *req) {

    uint32_t packet_index = start;
    char *cfdp = "cfdp";
    uint32_t len = strnlen(cfdp, 5);
    
    while (packet_index < data_length - len) {

        if (strncmp(&packet[packet_index], cfdp, len) != 0)
            break;

        packet_index = get_message_from_packet(packet, packet_index, req);
    }
    return packet_index;
}
