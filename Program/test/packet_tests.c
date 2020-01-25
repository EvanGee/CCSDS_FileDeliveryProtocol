
#define TESTING 1

#include "test.h"
#include "utils.h"
#include <stdio.h>
#include "filesystem_funcs.h"
#include "port.h"
#include "protocol_handler.h"
#include "mib.h"
#include "file_delivery_app.h"
#include "packet.h"
#include "unit_tests.h"
#include "requests.h"
#include "stdlib.h"

#define PACKET_TEST_SIZE 2000 

/*
typedef struct pdu_eof {
    unsigned int condition_code : 4;
    unsigned int spare : 4;
    uint32_t checksum;

    uint32_t file_size;

    //Omitted if condition code is ‘No error’. Otherwise, entity ID in the
    //TLV is the ID of the entity at which transaction cancellation was
    //initiated.
    TLV fault_location;
    
} Pdu_eof;
*/

static int test_build_eof_packet(char *packet, int packet_start) {

    DECLARE_NEW_TEST("testing eof_packet");

    File *file = create_file("dest.jpg", false);

    //need to set partialcheckus to checksum, because it gets set from reading in data
    file->partial_checksum = check_sum_file(file, 1000);
    
    build_eof_packet(packet, packet_start, file->total_size, file->partial_checksum);

    int packet_index = packet_start;
    Pdu_directive *directive = (Pdu_directive *) &packet[packet_index];
    directive->directive_code = EOF_PDU;
    packet_index++;

    Pdu_eof *eof_pdu = (Pdu_eof*) &packet[packet_index];
    ASSERT_EQUALS_INT("condition_code should equal NO_ERROR", eof_pdu->condition_code, COND_NO_ERROR);   
    ASSERT_EQUALS_INT("filesize should equal", htonl(eof_pdu->file_size), file->total_size);
    ASSERT_EQUALS_INT("checksum should equal", eof_pdu->checksum, file->partial_checksum);

    free_file(file);

    //testing this
    return 0;
}

static int test_respond_to_naks(char *packet, uint32_t packet_index) {
    Request *req = init_request(5000);

    ssp_cleanup_req(req);
    return 0;
}

static int test_respond_metadata_request() {

    return 0;
}

static void test_build_data_packet(char *packet, uint32_t packet_index){

    DECLARE_NEW_TEST("testing data packet");

    File *file = create_file("testfile", 0);

    build_data_packet(packet, packet_index, file, 1000);

    ASSERT_EQUALS_INT("test proper datapacket offset set", (uint64_t)packet[packet_index], 0);
    ASSERT_EQUALS_STR("test proper datapacket creation", &packet[packet_index + 4], "tempfileyo", 10);
    
    ASSERT_EQUALS_INT("should equal 100", packet_index + 87 + 4, 100);
    build_data_packet(packet, packet_index, file, 1000);
    
    uint32_t offset_in_packet = 0;
    memcpy(&offset_in_packet, &packet[packet_index], 4);
    ASSERT_EQUALS_INT("test proper datapacket offset set", offset_in_packet, 10);

    free_file(file);
}


static void nak_print(void *element, void *args){
    Offset *offset = (Offset *)element;
    ssp_printf("start: %u end: %u\n", offset->start, offset->end);
}

static int test_build_nak_packet(char* packet, uint32_t start) {

    DECLARE_NEW_TEST("testing build nak packet");
    Request *req = init_request(5000);

    req->file_size = 100000;
    memcpy(req->destination_file_name, "testestest", 15);
    memcpy(req->source_file_name, "someotherfile", 15);

    process_file_request_metadata(req);

    uint64_t count = req->file->missing_offsets->count;

    uint32_t data_len = build_nak_packet(packet, start, req);

    ASSERT_EQUALS_INT("NAK directive code set", packet[start], NAK_PDU);
    //25 = start_scope + end_scope + 1 offset + 1byte NAK_PDU code
    ASSERT_EQUALS_INT("length of packet", 25, data_len);
    uint32_t packet_index = start + 1;
    Pdu_nak *nak = (Pdu_nak *) &packet[packet_index];
    
    uint32_t start_scope = ntohl(nak->start_scope);
    uint32_t end_scope = ntohl(nak->end_scope);

    ASSERT_EQUALS_INT("start offset == 0 ", start_scope, 0);
    ASSERT_EQUALS_INT("end scope == 100000 ", end_scope, 100000);

    uint64_t number_of_segments = ntohll(nak->segment_requests);
    ASSERT_EQUALS_INT("number of segments == 1 ", number_of_segments, 1);

    Offset offset[count];
    memcpy(offset, &nak->segments, sizeof(Offset) * count);
    start_scope = ntohl(offset->start);
    end_scope = ntohl(offset->end);

    ASSERT_EQUALS_INT("start offset == 0 ", start_scope, 0);
    ASSERT_EQUALS_INT("end scope == 100000 ", end_scope, 100000);


    receive_offset(req->file, 0, 1250, 5000);
    receive_offset(req->file, 0, 6000, 9000);
    receive_offset(req->file, 0, 10000, 15000);

    data_len = build_nak_packet(packet, start, req);
    set_data_length(packet, data_len);

    number_of_segments = ntohll(nak->segment_requests);

    ASSERT_EQUALS_INT("number of segments == 4 ", number_of_segments, 4);

    start_scope = ntohl(nak->start_scope);
    end_scope = ntohl(nak->end_scope);
    ASSERT_EQUALS_INT("correct packet start", start_scope, 0);
    ASSERT_EQUALS_INT("correct packet end", end_scope, 100000);
    packet_index += 16;

    //outgoing_packet_index
    memcpy(&start_scope, &packet[packet_index], 4);
    start_scope = ntohl(start_scope);
    packet_index += 4;
    memcpy(&end_scope, &packet[packet_index], 4);
    end_scope = ntohl(end_scope);
    packet_index += 4;
    ASSERT_EQUALS_INT("correct packet offset 0 start", start_scope, 0);
    ASSERT_EQUALS_INT("correct packet offset 0 end", end_scope, 1250);
    
    memcpy(&start_scope, &packet[packet_index], 4);
    start_scope = ntohl(start_scope);
    packet_index += 4;
    memcpy(&end_scope, &packet[packet_index], 4);
    end_scope = ntohl(end_scope);
    packet_index += 4;
    ASSERT_EQUALS_INT("correct packet offset 1 start", start_scope, 5000);
    ASSERT_EQUALS_INT("correct packet offset 1 end", end_scope, 6000);

    memcpy(&start_scope, &packet[packet_index], 4);
    start_scope = ntohl(start_scope);
    packet_index += 4;
    memcpy(&end_scope, &packet[packet_index], 4);
    end_scope = ntohl(end_scope);
    packet_index += 4;
    ASSERT_EQUALS_INT("correct packet offset 2 start", start_scope, 9000);
    ASSERT_EQUALS_INT("correct packet offset 2 end", end_scope, 10000);

    memcpy(&start_scope, &packet[packet_index], 4);
    start_scope = ntohl(start_scope);
    packet_index += 4;
    memcpy(&end_scope, &packet[packet_index], 4);
    end_scope = ntohl(end_scope);
    packet_index += 4;
    ASSERT_EQUALS_INT("correct packet offset 3 start", start_scope, 15000);
    ASSERT_EQUALS_INT("correct packet offset 3 end", end_scope, 100000);

    Pdu_header *header = (Pdu_header*) packet;

    ASSERT_EQUALS_INT("correct packet data_len", data_len, ntohs(header->PDU_data_field_len));

    ssp_cleanup_req(req);
    return 0;
}

int test_build_ack_finished_pdu(char *packet, uint32_t start) {

    DECLARE_NEW_TEST("testing finish ack packet");
    printf("testing finished ack creation\n");
    Request *req;

    Pdu_directive *pdu_d = (Pdu_directive *)&packet[start];

    ASSERT_EQUALS_INT("ACK_PDU directive correct", pdu_d->directive_code, ACK_PDU);

    Pdu_ack *ack = (Pdu_ack *)&packet[start + 1];
    ASSERT_EQUALS_INT("EOF_PDU directive correct", EOF_PDU, ack->directive_code);
    ASSERT_EQUALS_INT("COND_NO_ERROR correct", COND_NO_ERROR, ack->condition_code);
    ASSERT_EQUALS_INT("ACK_FINISHED_END correct", ack->directive_subtype_code, ACK_FINISHED_END);

    return 0;
}


int test_build_ack_eof_pdu(char *packet, uint32_t start) {
    //empty request

    DECLARE_NEW_TEST("testing eof ack packet");

    Request *req;
    uint8_t len =  build_ack (packet, start, EOF_PDU);

    Pdu_directive *pdu_d = (Pdu_directive *) &packet[start];
    ASSERT_EQUALS_INT("ACK_PDU directive correct", pdu_d->directive_code, ACK_PDU);

    Pdu_ack *ack = (Pdu_ack *)&packet[start + 1];
    ASSERT_EQUALS_INT("EOF_PDU directive correct", EOF_PDU, ack->directive_code);
    ASSERT_EQUALS_INT("COND_NO_ERROR correct", COND_NO_ERROR, ack->condition_code);
    ASSERT_EQUALS_INT("ACK_FINISHED_END correct", ack->directive_subtype_code, ACK_FINISHED_END);

    return 0;
}

int test_build_pdu_header(char *packet, Pdu_header *header, uint64_t sequence_number) {


    DECLARE_NEW_TEST("testing header creation");
    uint8_t length = build_pdu_header(packet, sequence_number, 0, header);
    uint32_t packet_index = PACKET_STATIC_HEADER_LEN;

    ASSERT_EQUALS_INT("packet length: ", length, 9);
    ASSERT_EQUALS_STR("packet source id ", &packet[packet_index], &header->source_id, header->length_of_entity_IDs);


    packet_index += header->length_of_entity_IDs;

    ASSERT_NOT_EQUALS_INT("transaction_sequence_number correctly placed ", packet[packet_index], sequence_number);
    packet_index += header->transaction_seq_num_len;

    ASSERT_NOT_EQUALS_STR("packet destination not equal to source id ", &packet[packet_index], &header->source_id, header->length_of_entity_IDs);
    ASSERT_EQUALS_STR("packet destination id ", &packet[packet_index], &header->destination_id, header->length_of_entity_IDs);

    packet_index += header->length_of_entity_IDs;
    Pdu_header *new_header = (Pdu_header *)packet;

    ASSERT_EQUALS_INT("CRC = CRC", header->CRC_flag, new_header->CRC_flag);
    ASSERT_EQUALS_INT("direction = direction", header->direction, new_header->direction);
    ASSERT_EQUALS_INT("length_of_entity_IDs = length_of_entity_IDs ", header->length_of_entity_IDs, new_header->length_of_entity_IDs);
    ASSERT_EQUALS_INT("PDU data_field_len = PDU_data_field_len ", header->PDU_data_field_len, new_header->PDU_data_field_len);
    ASSERT_EQUALS_INT("PDU_type = PDU_type", header->PDU_type, new_header->PDU_type);
    ASSERT_EQUALS_INT("reserved_bit_0 = reserved_bit_0 ", header->reserved_bit_0, new_header->reserved_bit_0);
    ASSERT_EQUALS_INT("reserved_bit_1 = reserved_bit_1 ", header->reserved_bit_1, new_header->reserved_bit_1);
    ASSERT_EQUALS_INT("reserved_bit_2 = reserved_bit_2 ", header->reserved_bit_2, new_header->reserved_bit_2);
    ASSERT_EQUALS_INT("version = version", header->version, new_header->version);
    ASSERT_EQUALS_INT("transaction_seq_num_len = transaction_seq_num_len ", header->transaction_seq_num_len,  new_header->transaction_seq_num_len);
    ASSERT_EQUALS_INT("transmission_mode = transmission_mode ", 0, new_header->transmission_mode);
    ASSERT_EQUALS_INT("total header length equal ", packet_index, length);

    return packet_index;
}


int test_build_metadata_packet(char *packet, uint32_t start) {

    memset(&packet[start], 0, 20);
    DECLARE_NEW_TEST("testing metadata packets");

    Request *req = init_request(1000);
    Request *recv_request = init_request(1000);

    uint8_t len = build_put_packet_metadata(packet, start, req);
    fill_request_pdu_metadata(&packet[start + 1], recv_request);    

    ASSERT_EQUALS_INT("test packet filesize", req->file_size, recv_request->file_size);
    ASSERT_EQUALS_STR("test src_file_name", req->source_file_name, recv_request->source_file_name, strnlen(req->source_file_name, MAX_PATH));
    ASSERT_EQUALS_STR("test dest_file_name", req->destination_file_name, recv_request->destination_file_name,  strnlen(req->source_file_name, MAX_PATH));
    
    char *str = "HELLO WORLD";

    memcpy(req->destination_file_name, str, strnlen(str, MAX_PATH) );
    memcpy(req->source_file_name, str, strnlen(str, MAX_PATH) );

    len = build_put_packet_metadata(packet, start, req);
    fill_request_pdu_metadata(&packet[start + 1], recv_request);

    ASSERT_EQUALS_INT("test packet filesize", req->file_size, recv_request->file_size);
    ASSERT_EQUALS_STR("test src_file_name", req->source_file_name, recv_request->source_file_name, strnlen(req->source_file_name, MAX_PATH));
    ASSERT_EQUALS_STR("test dest_file_name", req->destination_file_name, recv_request->destination_file_name,  strnlen(req->source_file_name, MAX_PATH));
    
    uint16_t data_len = get_data_length(packet);
    ASSERT_EQUALS_INT("test metadata set data length", data_len, len-start);
    
    ssp_cleanup_req(req);
    ssp_cleanup_req(recv_request);

    return 0;
}


int test_add_messages_to_packet(char *packet, uint32_t start) {

    char *dest = "dest";
    char *src = "src";
    uint32_t id = 2;
    uint8_t len = 1;

    uint32_t packet_index = start;
    DECLARE_NEW_TEST("testing add_messages_to_packet");

    Request *req = init_request(1000);
    int error = add_proxy_message_to_request(id, len, src, dest, req);

    memset(&packet[start], 0, 100);
    packet_index = add_messages_to_packet(packet, packet_index, req->messages_to_user);

    ASSERT_EQUALS_STR("'cfdp' should be at the start of the message", &packet[start], "cfdp", 5);
    ASSERT_EQUALS_INT("testing PROXY_PUT_REQUEST code", (uint8_t) packet[start + 5], PROXY_PUT_REQUEST);

    LV* dest_file, *src_file, *dest_id;

    packet_index = start + 6;
    dest_id = copy_lv_from_buffer(packet, packet_index);
    ASSERT_EQUALS_INT("dest_file.length", dest_id->length, len);
    ASSERT_EQUALS_INT("dest_file.value", *(uint8_t*) (dest_id->value), id);
    packet_index += dest_id->length + 1;
    free_lv(dest_id);

    
    src_file = copy_lv_from_buffer(packet, packet_index);
    ASSERT_EQUALS_INT("src_file.length", src_file->length, strnlen(src, 100) + 1);
    ASSERT_EQUALS_STR("src_file.value", src, (char *) src_file->value, src_file->length);
    packet_index += src_file->length + 1;
    free_lv(src_file);
    

    dest_file = copy_lv_from_buffer(packet, packet_index);
    ASSERT_EQUALS_INT("dest_file.length", dest_file->length, strnlen(dest, 100) + 1);
    ASSERT_EQUALS_STR("dest_file.value", dest, (char *)dest_file->value, dest_file->length);
    free_lv(dest_file);

    ssp_cleanup_req(req);
    return 0;
}

int test_get_message_from_packet(char *packet, uint32_t start) {

    char *dest = "dest";
    char *src = "src";
    uint32_t id = 2;
    uint8_t len = 1;

    uint32_t packet_index = start;

    Request *req = init_request(1000);
    int error = add_proxy_message_to_request(id, len, src, dest, req);

    uint32_t length_of_message = add_messages_to_packet(packet, start, req->messages_to_user);

    Request *req2 = init_request(1000);
    uint32_t next_message = get_message_from_packet(packet, start, req2);

    Message *m = req2->messages_to_user->pop(req2->messages_to_user);
    Message_put_proxy *p_message = m->value;

    ASSERT_EQUALS_INT("dest_file.length", p_message->destination_file_name->length, strnlen(dest, 100) + 1);
    ASSERT_EQUALS_STR("dest_file.value", p_message->destination_file_name->value, dest, strnlen(dest, 100));

    ASSERT_EQUALS_INT("src_file.length",  p_message->source_file_name->length, strnlen(src, 100) + 1);
    ASSERT_EQUALS_STR("src_file.value", src, p_message->source_file_name->value, strnlen(src, 100));

    ASSERT_EQUALS_INT("dest_id.length", p_message->destination_id->length, len);
    ASSERT_EQUALS_INT("dest_id.value", *(uint8_t*)p_message->destination_id->value, id);

    ASSERT_EQUALS_INT("next message should be at index ", next_message, length_of_message);

    ssp_free_message(m);
    ssp_cleanup_req(req);
    ssp_cleanup_req(req2);
    return 0;
    
}


//test multiple messages
int test_get_messages_from_packet(char *packet, uint32_t start) {

    char *dest = "dest";
    char *src = "src";
    uint32_t id = 2;
    uint8_t len = 1;

    uint32_t packet_index = start;

    Request *req = init_request(1000);
    
    
    int error = add_proxy_message_to_request(id, len, src, dest, req);

    uint32_t length_of_message = add_messages_to_packet(packet, start, req->messages_to_user);
    length_of_message = add_messages_to_packet(packet, length_of_message, req->messages_to_user);
    length_of_message = add_messages_to_packet(packet, length_of_message, req->messages_to_user);

    Request *req2 = init_request(1000);
    get_messages_from_packet(packet, start, 1000 - start, req2);

    int message_count = req2->messages_to_user->count;

    for (int i = 0; i < message_count; i++) {

        Message *message = req2->messages_to_user->pop(req2->messages_to_user);
        
        if (message->header.message_type == PROXY_PUT_REQUEST) {

            Message_put_proxy *p_message = (Message_put_proxy *) message->value;
            ASSERT_EQUALS_INT("received proxy messages: dest.id", *(uint8_t*) p_message->destination_id->value, id);
            ASSERT_EQUALS_STR("received proxy messages: src file", src,  (char *) p_message->source_file_name->value, p_message->source_file_name->length);
            ASSERT_EQUALS_STR("received proxy messages: dest file", dest, (char *) p_message->destination_file_name->value, p_message->destination_file_name->length);    
            
        }
        ssp_free_message(message);
    }

    ssp_cleanup_req(req);
    ssp_cleanup_req(req2);

}


int packet_tests() {

    printf("starting Packet Tests (creating and changing packet values)\n");


    //setting host name for testing
    char *host_name = "127.0.0.1";
    uint32_t addr[sizeof(uint32_t)];
    inet_pton(AF_INET, host_name, addr);
    
    char *packet = calloc(PACKET_TEST_SIZE, sizeof(char));
    uint64_t sequence_number = 12345663234;

    Pdu_header pdu_header;
    Remote_entity remote_entity;

    int error = get_remote_entity_from_json (&remote_entity, 1);
    get_header_from_mib2(&pdu_header, remote_entity, 2);

    int data_start_index = test_build_pdu_header(packet, &pdu_header, sequence_number);
    /*
    test_build_ack_eof_pdu(packet, data_start_index);
    test_build_nak_packet(packet, data_start_index);
    test_respond_to_naks(packet, data_start_index);

    memset(packet, 0, PACKET_TEST_SIZE);
    data_start_index = test_build_pdu_header(packet, &pdu_header, sequence_number);

    test_build_data_packet(packet, data_start_index);
    test_build_metadata_packet(packet, data_start_index);
    test_build_eof_packet(packet, data_start_index);
    
    test_add_messages_to_packet(packet, data_start_index);
    test_get_message_from_packet(packet, data_start_index);
    test_get_messages_from_packet(packet, data_start_index);
    
    */
    ssp_cleanup_pdu_header(&pdu_header);
    ssp_free(packet);
    return 0;

}


