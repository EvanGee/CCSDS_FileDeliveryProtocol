
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

static int test_build_eof_packet(char *packet, int packet_start) {

    DECLARE_NEW_TEST("testing eof_packet");

    File *file = create_file("dest.jpg", false);

    //need to set partialcheckus to checksum, because it gets set from reading in data
    file->partial_checksum = check_sum_file(file, 1000);
    memset(&packet[packet_start], 0, 10);

    file->partial_checksum = 1231251;
    file->total_size = 141254;
    
    build_eof_packet(packet, packet_start, file->total_size, file->partial_checksum);

    int packet_index = packet_start;

    uint8_t directive = packet[packet_index];
    ASSERT_EQUALS_INT("EOF_PDU directive correct", directive, EOF_PDU);
    
    packet_index++;

    Pdu_eof eof;
    
    get_eof_from_packet(&packet[packet_index], &eof);

    ASSERT_EQUALS_INT("condition_code should equal COND_NO_ERROR", eof.condition_code, COND_NO_ERROR);   
    ASSERT_EQUALS_INT("filesize should equal", eof.file_size, file->total_size);
    ASSERT_EQUALS_INT("checksum should equal", eof.checksum, file->partial_checksum);

    ssp_free_file(file);

    //testing this
    return 0;
}


static int test_respond_metadata_request() {

    return 0;
}

static void test_build_data_packet(char *packet, uint32_t packet_index){

    DECLARE_NEW_TEST("testing data packet");

    File *file = create_file("test_files/testfile", 0);

    create_data_burst_packets(packet, packet_index, file, 1500);

    uint32_t offset = get_data_offset_from_packet(&packet[packet_index]);

    ASSERT_EQUALS_INT("test proper datapacket offset set", offset, 0);
    ASSERT_EQUALS_STR("test proper datapacket creation", &packet[packet_index + 4], "testfileyo", 10);
    
    ssp_free_file(file);
}


static void nak_print(Node *node, void *element, void *args){
    Offset *offset = (Offset *)element;
    ssp_printf("start: %u end: %u\n", offset->start, offset->end);
}

static int test_build_nak_packet(char* packet, uint32_t start) {

    DECLARE_NEW_TEST("testing build nak packet");
    Request *req = mock_empty_request();

    req->file_size = 100000;
    ssp_memcpy(req->destination_file_name, "testestest", 15);
    ssp_memcpy(req->source_file_name, "someotherfile", 15);

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

    uint64_t number_of_segments = ssp_ntohll(nak->segment_requests);
    ASSERT_EQUALS_INT("number of segments == 1 ", number_of_segments, 1);

    Offset offset[count];
    ssp_memcpy(offset, &nak->segments, sizeof(Offset) * count);
    start_scope = ntohl(offset->start);
    ssp_printf("test start_scope %d\n", start_scope);
    end_scope = ntohl(offset->end);
    ssp_printf("test end_scope %d\n", end_scope);

    ASSERT_EQUALS_INT("start offset == 0 ", start_scope, 0);
    ASSERT_EQUALS_INT("end scope == 100000 ", end_scope, 100000);

    receive_offset(req->file, 1250, 5000);
    receive_offset(req->file, 6000, 9000);
    receive_offset(req->file, 10000, 15000);

    data_len = build_nak_packet(packet, start, req);
    set_data_length(packet, data_len);

    number_of_segments = ssp_ntohll(nak->segment_requests);

    ASSERT_EQUALS_INT("number of segments == 4 ", number_of_segments, 4);

    start_scope = ntohl(nak->start_scope);
    end_scope = ntohl(nak->end_scope);
    ASSERT_EQUALS_INT("correct packet start", start_scope, 0);
    ASSERT_EQUALS_INT("correct packet end", end_scope, 100000);
    packet_index += 16;

    //outgoing_packet_index
    ssp_memcpy(&start_scope, &packet[packet_index], 4);
    start_scope = ntohl(start_scope);
    packet_index += 4;
    ssp_memcpy(&end_scope, &packet[packet_index], 4);
    end_scope = ntohl(end_scope);
    packet_index += 4;
    ASSERT_EQUALS_INT("correct packet offset 0 start", start_scope, 0);
    ASSERT_EQUALS_INT("correct packet offset 0 end", end_scope, 1250);
    
    ssp_memcpy(&start_scope, &packet[packet_index], 4);
    start_scope = ntohl(start_scope);
    packet_index += 4;
    ssp_memcpy(&end_scope, &packet[packet_index], 4);
    end_scope = ntohl(end_scope);
    packet_index += 4;
    ASSERT_EQUALS_INT("correct packet offset 1 start", start_scope, 5000);
    ASSERT_EQUALS_INT("correct packet offset 1 end", end_scope, 6000);

    ssp_memcpy(&start_scope, &packet[packet_index], 4);
    start_scope = ntohl(start_scope);
    packet_index += 4;
    ssp_memcpy(&end_scope, &packet[packet_index], 4);
    end_scope = ntohl(end_scope);
    packet_index += 4;
    ASSERT_EQUALS_INT("correct packet offset 2 start", start_scope, 9000);
    ASSERT_EQUALS_INT("correct packet offset 2 end", end_scope, 10000);

    ssp_memcpy(&start_scope, &packet[packet_index], 4);
    start_scope = ntohl(start_scope);
    packet_index += 4;
    ssp_memcpy(&end_scope, &packet[packet_index], 4);
    end_scope = ntohl(end_scope);
    packet_index += 4;
    ASSERT_EQUALS_INT("correct packet offset 3 start", start_scope, 15000);
    ASSERT_EQUALS_INT("correct packet offset 3 end", end_scope, 100000);

    ASSERT_EQUALS_INT("correct packet data_len", data_len, get_data_length(packet));

    ssp_cleanup_req(req);
    return 0;
}



static int test_receive_end_offset(File *file){
    add_first_offset(file, file->total_size);
    ASSERT_EQUALS_INT("file filesize", 150033, file->total_size);
    ASSERT_EQUALS_INT("offset length should be 1", 1, file->missing_offsets->count);

    receive_offset(file, 100000, 150033);

    ASSERT_EQUALS_INT("offset length should be 1 after insert end", 1, file->missing_offsets->count);
    Offset *of =(Offset *) file->missing_offsets->pop(file->missing_offsets);
    ASSERT_EQUALS_INT("offset start should be 0 after insert end", 0, of->start);
    ASSERT_EQUALS_INT("offset end should be 100000 after insert end", 100000, of->end);
    ssp_free(of);
}


static int test_receive_start_offset(File *file){
    add_first_offset(file, file->total_size);
    ASSERT_EQUALS_INT("file filesize", 150033, file->total_size);
    ASSERT_EQUALS_INT("offset length should be 1", 1, file->missing_offsets->count);

    receive_offset(file, 0, 10000);

    ASSERT_EQUALS_INT("offset length should be 1 after insert end", 1, file->missing_offsets->count);
    Offset *of =(Offset *) file->missing_offsets->pop(file->missing_offsets);
    ASSERT_EQUALS_INT("offset start should be 10000 after insert start", 10000, of->start);
    ASSERT_EQUALS_INT("offset end should be 150033 after insert start", file->total_size, of->end);
    ssp_free(of);
}


static int test_receive_middle_offset(File *file){
    add_first_offset(file, file->total_size);
    ASSERT_EQUALS_INT("file filesize", 150033, file->total_size);
    ASSERT_EQUALS_INT("offset length should be 1", 1, file->missing_offsets->count);

    receive_offset(file, 5000, 10000);

    ASSERT_EQUALS_INT("offset length should be 1 after insert end", 2, file->missing_offsets->count);
    Offset *of =(Offset *) file->missing_offsets->pop(file->missing_offsets);
    
    ASSERT_EQUALS_INT("offset start should be 10000 after insert mid", 10000, of->start);
    ASSERT_EQUALS_INT("offset end should be 150033 after insert mid", file->total_size, of->end);

    ASSERT_EQUALS_INT("offset length should be 1 after insert end", 1, file->missing_offsets->count);
    ssp_free(of);
    of =(Offset *) file->missing_offsets->pop(file->missing_offsets);
    
    ASSERT_EQUALS_INT("offset start should be 0 after insert mid", 0, of->start);
    ASSERT_EQUALS_INT("offset end should be 5000 after insert mid", 5000, of->end);
    ssp_free(of);
}


static int test_receive_parts_offset(File *file){
    add_first_offset(file, file->total_size);
    ASSERT_EQUALS_INT("file filesize", 150033, file->total_size);
    ASSERT_EQUALS_INT("offset length should be 1", 1, file->missing_offsets->count);

    receive_offset(file, 500, 1000);
    receive_offset(file, 1000, 10000);
    receive_offset(file, 0, 500);
    receive_offset(file, 10000, 10500);
    receive_offset(file, 10500, 150033);


    ASSERT_EQUALS_INT("offset length should be 0 after insert end", 0, file->missing_offsets->count);
}

static int test_receive_final_offset(File *file){
    add_first_offset(file, file->total_size);
    ASSERT_EQUALS_INT("file filesize", 150033, file->total_size);
    ASSERT_EQUALS_INT("offset length should be 1", 1, file->missing_offsets->count);

    receive_offset(file, 0, 150033);
    ASSERT_EQUALS_INT("offset length should be 0 after insert end", 0, file->missing_offsets->count);

    add_first_offset(file, file->total_size);
    receive_offset(file, 0, 100000);
    receive_offset(file, 100000, 150032);
    receive_offset(file, 150032, 150033);

    ASSERT_EQUALS_INT("offset length should be 0 after insert end", 0, file->missing_offsets->count);
}

static int test_nak_print(File *file){

    add_first_offset(file, file->total_size);
    ASSERT_EQUALS_INT("file filesize", 150033, file->total_size);
    ASSERT_EQUALS_INT("offset length should be 1", 1, file->missing_offsets->count);


    receive_offset(file, 148900, 150033);

    file->missing_offsets->iterate(file->missing_offsets, nak_print, NULL);
}

static int test_receive_offset(){

    File *file = create_file("test_files/test_receive.jpg", 0);
    test_receive_end_offset(file);
    test_receive_start_offset(file);
    test_receive_middle_offset(file);
    test_receive_parts_offset(file);
    test_receive_final_offset(file);
    test_nak_print(file);



}

static int test_build_very_large_nak_packet(char* packet, uint32_t start) {

    DECLARE_NEW_TEST("testing build very large nak packet");
    
    File *file = create_file("test_files/vid.mp4", 0);
    Request *req = mock_empty_request();

    req->file = file;
    req->file_size = file->total_size;
    process_file_request_metadata(req);

    //fail receiving weird offsets
    for (int i = 0; i < 10000; i+=10) {
        receive_offset(file, i, i+10);
        i++;
    }
    uint64_t count = file->missing_offsets->count;

    uint32_t data_len = build_nak_packet(packet, start, req);
    ssp_printf("data length%d\n", data_len);
    ssp_printf("start%d\n", start);

    uint32_t offsets_sent = (data_len - 17) / sizeof(Offset);

    ssp_printf("offsets that fit in packet %d\n", offsets_sent);

    ASSERT_EQUALS_INT("data length of NAK fits in the packet", 1489, data_len);

    ssp_printf("data_len = %d", data_len);

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
    memset(&packet[start], 0, 10);

    uint8_t len = build_ack(packet, start, EOF_PDU);

    uint8_t directive = packet[start];
    ASSERT_EQUALS_INT("ACK_PDU directive correct", directive, ACK_PDU);
    uint32_t packet_index = start + 1;

    Pdu_ack ack;
    get_ack_from_packet(&packet[packet_index], &ack);

    ASSERT_EQUALS_INT("EOF_PDU directive correct", EOF_PDU, ack.directive_code);
    ASSERT_EQUALS_INT("COND_NO_ERROR correct", COND_NO_ERROR, ack.condition_code);
    ASSERT_EQUALS_INT("ACK_FINISHED_END correct", ack.directive_subtype_code, ACK_FINISHED_END);

    return 0;
}

int test_build_pdu_header(char *packet, Pdu_header *header) {


    DECLARE_NEW_TEST("testing header creation");
    uint32_t packet_index = 0;
    //ssp_print_bits(header, 4);
    Pdu_header new_header;
    header->transaction_sequence_number = 325;
    header->transaction_seq_num_len = 2;

    memset(&new_header, 0, 4);
    
    int length = build_pdu_header(packet, header->transaction_sequence_number, header->transmission_mode, header->PDU_data_field_len, header);
    if (length < 0) {
        ssp_printf("failed to build pdu header\n");
    }

    ssp_print_bits(&packet[4], 10);
    get_pdu_header_from_packet(packet, &new_header);


    ASSERT_EQUALS_INT("CRC = CRC", header->CRC_flag, new_header.CRC_flag);
    ASSERT_EQUALS_INT("direction = direction", header->direction, new_header.direction);
    ASSERT_EQUALS_INT("length_of_entity_IDs = length_of_entity_IDs ", header->length_of_entity_IDs, new_header.length_of_entity_IDs);
    ASSERT_EQUALS_INT("PDU data_field_len = PDU_data_field_len ", header->PDU_data_field_len, new_header.PDU_data_field_len);
    ASSERT_EQUALS_INT("PDU_type = PDU_type", header->PDU_type, new_header.PDU_type);
    ASSERT_EQUALS_INT("reserved_bit_0 = reserved_bit_0 ", header->reserved_bit_0, new_header.reserved_bit_0);
    ASSERT_EQUALS_INT("reserved_bit_1 = reserved_bit_1 ", header->reserved_bit_1, new_header.reserved_bit_1);
    ASSERT_EQUALS_INT("reserved_bit_2 = reserved_bit_2 ", header->reserved_bit_2, new_header.reserved_bit_2);
    ASSERT_EQUALS_INT("version = version", header->version, new_header.version);
    ASSERT_EQUALS_INT("transaction_seq_num_len = transaction_seq_num_len ", header->transaction_seq_num_len,  new_header.transaction_seq_num_len);
    ASSERT_EQUALS_INT("transmission_mode = transmission_mode ", header->transmission_mode, new_header.transmission_mode);

    ASSERT_EQUALS_INT("packet length: ", length, (header->transaction_seq_num_len + (header->length_of_entity_IDs * 2) + 4));
    ASSERT_EQUALS_INT("packet source id ", header->source_id, new_header.source_id);

    ssp_printf("%d\n", new_header.transaction_sequence_number);
    ASSERT_EQUALS_INT("transaction_sequence_number correctly placed ", header->transaction_sequence_number, new_header.transaction_sequence_number);
    ASSERT_EQUALS_INT("packet destination id ",  header->destination_id, new_header.destination_id);

    return length;
}


int test_build_metadata_packet(char *packet, uint32_t start) {

    memset(&packet[start], 0, 20);
    DECLARE_NEW_TEST("testing metadata packets");

    Request *req = mock_request();
    Request *recv_request = mock_empty_request();

    req->file_size = 35;

    ssp_printf("%s\n", req->source_file_name);

    uint8_t len = build_put_packet_metadata(packet, start, req);
    parse_metadata_packet(packet, start + SIZE_OF_DIRECTIVE_CODE, recv_request);    

    ssp_printf("%s\n", recv_request->source_file_name);


    ASSERT_EQUALS_INT("test packet filesize", req->file_size, recv_request->file_size);
    ASSERT_EQUALS_STR("test src_file_name", req->source_file_name, recv_request->source_file_name, strnlen(req->source_file_name, MAX_PATH));
    ASSERT_EQUALS_STR("test dest_file_name", req->destination_file_name, recv_request->destination_file_name,  strnlen(req->source_file_name, MAX_PATH));
    char *str = "HELLO WORLDSSSSSSSSSSSSSS";

    ssp_memcpy(req->destination_file_name, str, strnlen(str, MAX_PATH) );
    ssp_memcpy(req->source_file_name, str, strnlen(str, MAX_PATH) );

    len = build_put_packet_metadata(packet, start, req);
    parse_metadata_packet(packet, start + SIZE_OF_DIRECTIVE_CODE, recv_request);

    ASSERT_EQUALS_INT("test packet filesize", req->file_size, recv_request->file_size);
    ASSERT_EQUALS_STR("test src_file_name", req->source_file_name, recv_request->source_file_name, strnlen(req->source_file_name, MAX_PATH));
    ASSERT_EQUALS_STR("test dest_file_name", req->destination_file_name, recv_request->destination_file_name,  strnlen(req->source_file_name, MAX_PATH));
    
    uint16_t data_len = get_data_length(packet);
    ASSERT_EQUALS_INT("test metadata set data length", data_len, len-start);
    

    ssp_cleanup_req(req);
    ssp_cleanup_req(recv_request);

    return 0;
}


int test_add_cont_part_to_packet(char *packet, uint32_t start){

    DECLARE_NEW_TEST("testing add_message_cont_part_to_packet");

    uint32_t packet_index = start;

    uint64_t dest_id_original = 5;
    uint64_t original_id_original = 33;
    uint64_t transaction_id_original = 66;

    Request *req = mock_empty_request();
    int error = add_cont_partial_message_to_request(dest_id_original,original_id_original,transaction_id_original,req);

    memset(&packet[start], 0, 100);
    packet_index = add_messages_to_packet(packet, packet_index, req->messages_to_user);

    ASSERT_EQUALS_STR("'cfdp' should be at the start of the message", &packet[start], "cfdp", 5);
    ASSERT_EQUALS_INT("testing CONTINUE_PARTIAL code", (uint8_t) packet[start + 5], CONTINUE_PARTIAL);

    uint64_t dest_id, original_id, transaction_id;

    packet_index = start + 6;
    error = copy_id_lv_from_packet(&packet[packet_index], &dest_id);
    if (error < 0) {
        ssp_printf("failed to copy contents from buffer \n");
        return -1;
    }

    ASSERT_EQUALS_INT("dest_id", dest_id, dest_id_original);
    packet_index += 1 + 1;

    error = copy_id_lv_from_packet(&packet[packet_index], &original_id);
    if (error < 0) {
        ssp_printf("failed to copy contents from buffer \n");
        return -1;
    }

    ASSERT_EQUALS_INT("original_id", original_id, original_id_original);
    packet_index += 1 + 1;
    
    error = copy_id_lv_from_packet(&packet[packet_index], &transaction_id);
    if (error < 0) {
        ssp_printf("failed to copy contents from buffer \n");
        return -1;
    }
    
    ASSERT_EQUALS_INT("transaction_id", transaction_id, transaction_id_original);
    ssp_cleanup_req(req);


    


}


int test_add_messages_to_packet(char *packet, uint32_t start) {

    char *dest = "dest";
    char *src = "src";
    uint32_t id = 2;
    uint8_t len = 1;

    uint32_t packet_index = start;
    DECLARE_NEW_TEST("testing add_messages_to_packet");

    Request *req = mock_empty_request();
    int error = add_proxy_message_to_request(id, len, src, dest, req);

    memset(&packet[start], 0, 100);
    packet_index = add_messages_to_packet(packet, packet_index, req->messages_to_user);

    ASSERT_EQUALS_STR("'cfdp' should be at the start of the message", &packet[start], "cfdp", 5);
    ASSERT_EQUALS_INT("testing PROXY_PUT_REQUEST code", (uint8_t) packet[start + 5], PROXY_PUT_REQUEST);

    LV dest_file, src_file, dest_id;

    packet_index = start + 6;
    copy_lv_from_buffer(&dest_id, packet, packet_index);
    ASSERT_EQUALS_INT("dest_file.length", dest_id.length, len);
    ASSERT_EQUALS_INT("dest_file.value", *(uint8_t*) (dest_id.value), id);
    packet_index += dest_id.length + 1;
    free_lv(dest_id);

    
    copy_lv_from_buffer(&src_file, packet, packet_index);
    ASSERT_EQUALS_INT("src_file.length", src_file.length, strnlen(src, 100) + 1);
    ASSERT_EQUALS_STR("src_file.value", src, (char *) src_file.value, src_file.length);
    packet_index += src_file.length + 1;
    free_lv(src_file);
    

    copy_lv_from_buffer(&dest_file, packet, packet_index);
    ASSERT_EQUALS_INT("dest_file.length", dest_file.length, strnlen(dest, 100) + 1);
    ASSERT_EQUALS_STR("dest_file.value", dest, (char *)dest_file.value, dest_file.length);
    free_lv(dest_file);

    ssp_cleanup_req(req);
    return 0;
}

int test_get_message_from_packet(char *packet, uint32_t start) {

    char *dest = "dest";
    char *src = "src";
    uint32_t id = 2;
    uint8_t len = 1;

    DECLARE_NEW_TEST("testing add_messages_from_packet");

    uint32_t packet_index = start;

    Request *req = mock_empty_request();
    int error = add_proxy_message_to_request(id, len, src, dest, req);

    uint32_t length_of_message = add_messages_to_packet(packet, start, req->messages_to_user);

    Request *req2 = mock_empty_request();
    uint32_t next_message = get_message_from_packet(packet, start, req2);

    Message *m = req2->messages_to_user->pop(req2->messages_to_user);
    Message_put_proxy *p_message = m->value;

    ASSERT_EQUALS_STR("dest_file.value", p_message->destination_file_name.value, dest, strnlen(dest, 100));
    ASSERT_EQUALS_STR("src_file.value", src, p_message->source_file_name.value, strnlen(src, 100));
    ASSERT_EQUALS_INT("dest_id.value", p_message->destination_id, id);

    ASSERT_EQUALS_INT("next message should be at index ", next_message, length_of_message);

    ssp_free_message(m);
    ssp_cleanup_req(req);
    ssp_cleanup_req(req2);
    return 0;
    
}

int test_get_cont_partial_from_packet(char *packet, uint32_t start){

    DECLARE_NEW_TEST("testing add_message_cont_part_to_packet");

    uint32_t packet_index = start;

    Request *req = mock_empty_request();
    int error = add_cont_partial_message_to_request(1,1,1,req);

    memset(&packet[start], 0, 100);
    add_messages_to_packet(packet, packet_index, req->messages_to_user);

    Request *req2 = mock_empty_request();
    get_message_from_packet(packet, packet_index, req2);

    Message *m = req2->messages_to_user->pop(req2->messages_to_user);
    Message_cont_part_request *p_message = (Message_cont_part_request *) m->value;


    ASSERT_EQUALS_INT("destination_id.value", p_message->destination_id, 1);

    ASSERT_EQUALS_INT("originator_id.value", p_message->originator_id, 1);

    ASSERT_EQUALS_INT("transaction_id.value", p_message->transaction_id, 1);



}

//test multiple messages
int test_get_messages_from_packet(char *packet, uint32_t start) {


    DECLARE_NEW_TEST("testing get_messages_from_packet");

    char *dest = "dest";
    char *src = "src";
    uint32_t id = 2;
    uint8_t len = 1;

    uint32_t packet_index = start;

    Request *req = mock_empty_request();
    
    
    int error = add_proxy_message_to_request(id, len, src, dest, req);

    uint32_t length_of_message = add_messages_to_packet(packet, start, req->messages_to_user);
    length_of_message = add_messages_to_packet(packet, length_of_message, req->messages_to_user);
    length_of_message = add_messages_to_packet(packet, length_of_message, req->messages_to_user);
    
    Request *req2 = mock_empty_request();
    get_messages_from_packet(packet, start, 1000 - start, req2);

    int message_count = req2->messages_to_user->count;

    for (int i = 0; i < message_count; i++) {

        Message *message = req2->messages_to_user->pop(req2->messages_to_user);
        
        if (message->header.message_type == PROXY_PUT_REQUEST) {

            Message_put_proxy *p_message = (Message_put_proxy *) message->value;
            ASSERT_EQUALS_INT("received proxy messages: dest.id",  p_message->destination_id, id);
            ASSERT_EQUALS_STR("received proxy messages: src file", src,  (char *) p_message->source_file_name.value, p_message->source_file_name.length);
            ASSERT_EQUALS_STR("received proxy messages: dest file", dest, (char *) p_message->destination_file_name.value, p_message->destination_file_name.length);    
            
        }
        
        ssp_free_message(message);
    }

    ssp_cleanup_req(req);
    ssp_cleanup_req(req2);

}

int test_build_finished_pdu(char *packet, uint32_t start) {


    DECLARE_NEW_TEST("testing creation and parsing of finished pdu");
    memset(&packet[start], 0, 100);
    
    uint32_t packet_index = start;

    uint32_t data_len = build_finished_pdu(packet, start);

    packet_index += SIZE_OF_DIRECTIVE_CODE;

    Pdu_finished fin;
    get_finished_pdu(&packet[packet_index], &fin);
    
    ASSERT_EQUALS_INT("condition_code set", fin.condition_code, COND_NO_ERROR);
    ASSERT_EQUALS_INT("delivery_code not set", fin.delivery_code, 0);
    ASSERT_EQUALS_INT("end_system_status not set", fin.end_system_status, 0);
    ASSERT_EQUALS_INT("file_status FILE_RETAINED_SUCCESSFULLY set", fin.file_status, FILE_RETAINED_SUCCESSFULLY);
}


int test_copying_lvs(){

    DECLARE_NEW_TEST("testing copying and reading LVs ");
    uint64_t id = 184;
    char packet[250];

    int error = copy_id_lv_to_packet(packet, id);

    uint64_t id_received = 0;
    uint64_t len = copy_id_lv_from_packet(packet, &id_received);
    if (len < 0) {
        ssp_printf("failed to copy contents from buffer \n");
        return -1;
    }
    ASSERT_EQUALS_INT("ids should equal 232", (uint32_t)id_received, 184);
    ASSERT_EQUALS_INT("len should be 1", len, 1);


    id = 6699;
    error = copy_id_lv_to_packet(packet, id);

    id_received = 0;
    len = copy_id_lv_from_packet(packet, &id_received);
    if (len < 0) {
        ssp_printf("failed to copy contents from buffer \n");
        return -1;
    }
    ASSERT_EQUALS_INT("ids should equal 6699", (uint32_t)id_received, 6699);
    ASSERT_EQUALS_INT("len should be 2", len, 2);

    id = 15406584;
    error = copy_id_lv_to_packet(packet, id);

    id_received = 0;
    len = copy_id_lv_from_packet(packet, &id_received);
    if (len < 0) {
        ssp_printf("failed to copy contents from buffer \n");
        return -1;
    }
    ASSERT_EQUALS_INT("ids should equal 15406584", (uint32_t)id_received, 15406584);
    ASSERT_EQUALS_INT("len should be 4", len, 4);
    id = 32500405;
    error = copy_id_lv_to_packet(packet, id);

    id_received = 0;
    len = copy_id_lv_from_packet(packet, &id_received);
    if (len < 0) {
        ssp_printf("failed to copy contents from buffer \n");
        return -1;
    }
    ASSERT_EQUALS_INT("ids should equal 32500405", (uint32_t)id_received, 32500405);
    ASSERT_EQUALS_INT("len should be 8", len, 8);
    
}
int packet_tests() {
    
    //setting host name for testing
    char *host_name = "127.0.0.1";
    uint32_t addr[sizeof(uint32_t)];
    inet_pton(AF_INET, host_name, addr);
    
    char packet[PACKET_TEST_SIZE];

    Pdu_header pdu_header;
    Remote_entity remote_entity;

    int error = get_remote_entity_from_json (&remote_entity, 1);
    get_header_from_mib(&pdu_header, remote_entity, 2);

    int data_start_index = test_build_pdu_header(packet, &pdu_header);
    /*
    error = test_copying_lvs();
    
    test_build_metadata_packet(packet, data_start_index);
    test_get_messages_from_packet(packet, data_start_index);
    test_add_cont_part_to_packet(packet, data_start_index);
    test_get_cont_partial_from_packet(packet, data_start_index);
    test_build_ack_eof_pdu(packet, data_start_index);
*/
    test_build_eof_packet(packet, data_start_index);
    
    test_build_data_packet(packet, data_start_index);
    
    //test_build_nak_packet(packet, data_start_index);
    test_receive_offset();
    /*
    test_build_finished_pdu(packet, data_start_index);
    test_add_messages_to_packet(packet, data_start_index);
    test_get_message_from_packet(packet, data_start_index);
    */
    //next up
    
    //Skip for now, will fix after connection server works
    //test_build_very_large_nak_packet(packet, data_start_index);
    return 0;

}


