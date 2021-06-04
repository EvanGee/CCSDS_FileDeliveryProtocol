#include "filesystem_funcs.h"
#include "unit_tests.h"
#include "port.h"
#include "mib.h"
#include "requests.h"
#include <fcntl.h>
#include "test.h"



static void nak_print(Node *node, void *element, void *args){
    Offset *offset = (Offset *)element;
    ssp_printf("start: %u end: %u\n", offset->start, offset->end);
}

static int receive_offset_tests(){

    File *file = create_temp_file("temp_test", 2000);
    receive_offset(file, 5, 50);
    
    file->missing_offsets->iterate(file->missing_offsets, nak_print, 0);
    receive_offset(file, 100, 1000);

    file->missing_offsets->iterate(file->missing_offsets, nak_print, 0);
    receive_offset(file, 50, 100);

    file->missing_offsets->iterate(file->missing_offsets, nak_print, 0);
    return 0;

}

void print_json(char *key, char *value, void *params) {

    ssp_printf("%s:%s\n", key, value);

}


int read_json_file_test() {

    int error = read_json("mib/peer_1.json", print_json, NULL);

    return 1;
}

int test_write_request_json() {

    DECLARE_NEW_TEST("test writing and getting json data");
    char *json_file = "test_incomeplete_file.json";
    Request req;

    req.file = NULL;
    req.file = create_file("test_files/dest.jpg", 0);
    
    strncpy(req.source_file_name, "FileName", 20);
    strncpy(req.destination_file_name, "DestinationFileName", 100);
    

    add_first_offset(req.file, 3000);
    receive_offset(req.file, 1000, 2000);
    
    req.my_cfdp_id = 5;
    req.dest_cfdp_id = 10;
    req.transaction_sequence_number = 56423487523;
    req.paused = 0;
    req.resent_eof = 2;
    req.resent_finished = 2;
    req.sent_first_data_round = 1;
    req.timeout_before_cancel = 50;
    req.timeout_before_journal = 40;
    req.transmission_mode = 1;
    req.file_size = 150;
    req.local_entity.EOF_recv_indication = 1;
    req.local_entity.EOF_sent_indication = 1;
    req.local_entity.file_segment_recv_indication = 1;
    req.local_entity.Metadata_recv_indication = 1;
    req.local_entity.Metadata_sent_indication = 1;
    req.local_entity.resumed_indication = 1;
    req.local_entity.suspended_indication = 2;
    req.local_entity.transaction_finished_indication = 1;

    int error = write_request_json(&req, json_file);
    if (error < 0) {
        ssp_printf("failed to write json test file\n");
    }
    
    Request req2;

    error = get_request_from_json(&req2, "test_incomeplete_file.json");
    if (error < 0) {
        ssp_printf("failed to read json test file\n");
        return -1;
    }

    ASSERT_EQUALS_INT("my_cfdp_id", req.my_cfdp_id, req2.my_cfdp_id);
    ASSERT_EQUALS_INT("dest_cfdp_id", req.dest_cfdp_id, req2.dest_cfdp_id);
    ASSERT_EQUALS_INT("transaction_sequence_number", req.transaction_sequence_number, req2.transaction_sequence_number);
    ASSERT_EQUALS_INT("paused", req.paused, req2.paused);
    ASSERT_EQUALS_STR("FileName", req.source_file_name, req2.source_file_name, strnlen(req.source_file_name, 100));
    ASSERT_EQUALS_STR("DestinationFileName", req.destination_file_name, req2.destination_file_name, strnlen(req.destination_file_name, 100));    
    ASSERT_EQUALS_INT("resent_eof", req.resent_eof, req2.resent_eof);
    ASSERT_EQUALS_INT("resent_finished", req.resent_finished, req2.resent_finished);
    ASSERT_EQUALS_INT("sent_first_data_round", req.sent_first_data_round, req2.sent_first_data_round);
    ASSERT_EQUALS_INT("transmission_mode", req.transmission_mode, req2.transmission_mode);
    ASSERT_EQUALS_INT("file_size", req.file_size, req2.file_size);
    ASSERT_EQUALS_INT("local_entity.EOF_recv_indication", req.local_entity.EOF_recv_indication, req2.local_entity.EOF_recv_indication);
    ASSERT_EQUALS_INT("local_entity.EOF_sent_indication", req.local_entity.EOF_sent_indication, req2.local_entity.EOF_sent_indication);
    ASSERT_EQUALS_INT("local_entity.file_segment_recv_indication", req.local_entity.file_segment_recv_indication, req2.local_entity.file_segment_recv_indication);
    ASSERT_EQUALS_INT("local_entity.Metadata_recv_indication", req.local_entity.Metadata_recv_indication, req2.local_entity.Metadata_recv_indication);
    ASSERT_EQUALS_INT("local_entity.Metadata_sent_indication", req.local_entity.Metadata_sent_indication, req2.local_entity.Metadata_sent_indication);
    ASSERT_EQUALS_INT("local_entity.resumed_indication", req.local_entity.resumed_indication, req2.local_entity.resumed_indication);
    ASSERT_EQUALS_INT("local_entity.suspended_indication", req.local_entity.suspended_indication, req2.local_entity.suspended_indication);
    ASSERT_EQUALS_INT("local_entity.transaction_finished_indication", req.local_entity.transaction_finished_indication, req2.local_entity.transaction_finished_indication);
    
    
    Offset *o1 = req2.file->missing_offsets->pop(req2.file->missing_offsets);
    ASSERT_EQUALS_INT("offset start", 2000,  o1->start);
    ASSERT_EQUALS_INT("offset end", 3000,  o1->end);
    ssp_free(o1);

    Offset *o2 = req2.file->missing_offsets->pop(req2.file->missing_offsets);
    ASSERT_EQUALS_INT("offset start", 0,  o2->start);
    ASSERT_EQUALS_INT("offset end", 1000,  o2->end);
    ssp_free(o2);
    

    ssp_free_file(req2.file);
    ssp_free_file(req.file);

    return error;
    
}




int file_system_tests() {
    int error = 0;

    error = test_write_request_json();

    return error;
}

