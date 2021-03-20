

#include "protocol_handler.h"
#include "stdlib.h"
#include "mib.h"
#include "test.h"
#include "file_delivery_app.h"
#include "unit_tests.h"
#include "port.h"
#include "requests.h"
#include "filesystem_funcs.h"


static int test_process_pdu_eof() {
//char *packet, Request *req, Response res
    
    DECLARE_NEW_TEST("testing process eof pdu");

    char packet[2000];
    
    //builds the packet at offset 10
    File *file = mock_eof_packet(packet, 1, 2, "test_files/dest.jpg");
    uint32_t first_checksum = file->partial_checksum;
    uint32_t first_file_size = file->total_size;
    
    Request *req = mock_empty_request();
    req->file = create_file("test_files/eof_test", true);

    process_pdu_eof(&packet[11], req, req->res);
    
    ASSERT_EQUALS_INT("received eof, increment EOF_rec_indication", req->local_entity.EOF_recv_indication, true);
    ASSERT_EQUALS_INT("received eof, checksum should equal", req->file->eof_checksum, first_checksum);
    ASSERT_EQUALS_INT("received eof, filesize should equal", req->file->total_size, first_file_size);
    
    ssp_cleanup_req(req);

    //test empty files
    file = mock_eof_packet(packet, 1, 2, "test_files/empty.txt");
    req = mock_empty_request();
    req->file = file;
    
    process_pdu_eof(&packet[11], req, req->res);
    
    ASSERT_EQUALS_INT("received eof, increment EOF_rec_indication", req->local_entity.EOF_recv_indication, true);
    ASSERT_EQUALS_INT("received eof, checksum should equal", req->file->eof_checksum, file->partial_checksum);
    ASSERT_EQUALS_INT("received eof, filesize should equal", req->file->total_size, file->total_size);

    ssp_cleanup_req(req);
    return 0;
}

//main state machine
int test_on_server_time_out()  {

    //Response res, Request *req
    Request *req = mock_request();
    req->paused = false;

    //no meta data received, sending request for new one (can't send NAKs yet, because we don't know filesize)
    on_server_time_out(req->res, req);
    req->local_entity.Metadata_recv_indication = true;
    on_server_time_out(req->res, req);

    req->local_entity.EOF_recv_indication = true;
    on_server_time_out(req->res, req);

    req->local_entity.transaction_finished_indication = true;
    on_server_time_out(req->res, req);

    on_server_time_out(req->res, req);

    ssp_cleanup_req(req);
    return 0;

}


static int test_wrong_id(FTP *app) {


    DECLARE_NEW_TEST("testing wrong id");

    int error = 0;
    Response *res = mock_response();
    char packet[2000];
    
    int packet_index = mock_packet(packet, 2, 1);

    Request **req_container = &app->current_request; 
    Pdu_header incoming_pdu_header;

    error = process_pdu_header(packet, true, &incoming_pdu_header, *res, req_container, app->request_list, app);
    error = ASSERT_EQUALS_INT("process pdu should error", -1, error);
    Request *req = (*req_container);

    error = ASSERT_NULL("Test wrong id, Request should be NULL", req);
    error = ASSERT_EQUALS_INT("Length of request list should be 0", app->request_list->count, 0);

    free(res);
    return error;
}

static int test_correct_id(FTP *app) {

    DECLARE_NEW_TEST("testing correct id");

    int error = 0;
    Response *res = mock_response();
    char packet[2000];
    int packet_index = mock_packet(packet, 1, 2);

    set_data_length(packet, 100);
    Request **req_container = &app->current_request; 
    Pdu_header incoming_pdu_header;

    process_pdu_header(packet, true, &incoming_pdu_header, *res, req_container, app->request_list, app);
    Request *req = (*req_container);

    error = ASSERT_NOT_NULL("Test request, Request should not be NULL", req);
    error = ASSERT_EQUALS_INT("Length of request should be 1", app->request_list->count, 1);
    error = ASSERT_EQUALS_INT("request transaction number should equal", req->transaction_sequence_number, 1);
    error = ASSERT_EQUALS_INT("source id should equal 2", req->dest_cfdp_id, 2);

    free(res);
    return error;
}

static int test_process_pdu_header() {
    char buff[1500];
    int error = 0;
    FTP *app = ssp_alloc(1, sizeof(FTP));

    app->request_list = linked_list();
    app->packet_len = 1500;
    app->buff = buff;
    app->my_cfdp_id = 1;

    error = test_wrong_id(app);
    error = test_correct_id(app);

    app->request_list->free(app->request_list, ssp_cleanup_req);
    ssp_free(app);

    return error;
}
int test_process_data_packet() {


    DECLARE_NEW_TEST("testing process data packet");

    char packet[1500];
    uint32_t start = 20;
    uint32_t packet_len = 120;
    memset(packet, 0, 1500);

    File *file = create_file("test_files/peer_0.json", 0);
    File *file2 = create_file("test_files/received_peer.json", 1);
    
    //mimics process_file_request_metadata function
    add_first_offset(file2, file->total_size);


    int error = 0;
    uint32_t data_len = 0;

    int i = 0;
    for (i = 0; i < 10; i++) {
        create_data_burst_packets(packet, start, file, packet_len);
        data_len = get_data_length(packet); 
        process_data_packet(&packet[start], data_len, file2);
        ASSERT_EQUALS_INT("Checksum parital calculations", file2->partial_checksum, file->partial_checksum);
    }

    uint32_t checksum = check_sum_file(file, packet_len-start-4);

    ASSERT_EQUALS_INT("Checksum", file2->partial_checksum, checksum);
    ASSERT_EQUALS_INT("Length of offset list should equal 1 ", file2->missing_offsets->count, 0);

    ssp_free_file(file);
    ssp_free_file(file2);

}

int test_process_nak() {


    DECLARE_NEW_TEST("testing nak client response");
    char packet[2000];
    uint32_t start = 20;
    uint32_t data_len = sizeof(packet) - start;
    memset(packet, 0, 1500);

    Request *req = mock_request();
    Response *res = mock_response();
    Client *client = mock_client();
    
    ssp_free_file(req->file);
    req->file = create_file("test_files/nak_test.jpg", 0);
    add_first_offset(req->file, req->file->total_size);

    build_nak_packet(packet, start, req);

    File *file = create_file(req->destination_file_name, 1);
    add_first_offset(file, req->file->total_size);
    
    process_nak_pdu(&packet[start + SIZE_OF_DIRECTIVE_CODE], req, *res, client);

    int error = 0;
    error = ASSERT_EQUALS_INT("Nak list count should be of size 1",  file->missing_offsets->count, 1);

    Offset *offset = (Offset*) file->missing_offsets->pop(file->missing_offsets);
    
    error = ASSERT_EQUALS_INT("offset start should be 0", offset->start, 0);
    error = ASSERT_EQUALS_INT("offset end should be file size", offset->end, 150033);

    ssp_free(offset);
    ssp_free_file(file);
    ssp_cleanup_req(req);
    ssp_cleanup_client(client);
    ssp_free(res);

    return error;
    
}

void test_send_data_from_nak_array(){


}

int protocol_handler_test() {
    int error = 0;
    //error = test_process_pdu_header();
    //error = test_process_pdu_eof();
    //error = test_on_server_time_out();
    //error = test_process_data_packet();
    error = test_process_nak();

    return error;
}