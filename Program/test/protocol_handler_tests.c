

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
    
    //test images
    File *file = mock_eof_packet(packet, 1, 2, "test_files/dest.jpg");
    Request *req = mock_empty_request();
    req->file = file;

    process_pdu_eof(&packet[10], req, req->res);
    
    ASSERT_EQUALS_INT("received eof, increment EOF_rec_indication", req->local_entity.EOF_recv_indication, true);
    ASSERT_EQUALS_INT("received eof, checksum should equal", req->file->eof_checksum, file->partial_checksum);
    ASSERT_EQUALS_INT("received eof, filesize should equal", req->file->total_size, file->total_size);
    
    ssp_cleanup_req(req);

    //test empty files
    file = mock_eof_packet(packet, 1, 2, "test_files/empty.txt");
    req = mock_empty_request();
    req->file = file;
    
    process_pdu_eof(&packet[10], req, req->res);

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
    req->procedure = sending_data;

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
    int error = 0;
    Response *res = mock_response();
    char packet[2000];
    
    int packet_index = mock_packet(packet, 2, 1);

    Request **req_container = &app->current_request; 

    error = process_pdu_header(packet, true, *res, req_container, app->request_list, app);
    error = ASSERT_EQUALS_INT("process pdu should error", -1, error);
    Request *req = (*req_container);

    error = ASSERT_NULL("Test wrong id, Request should be NULL", req);
    error = ASSERT_EQUALS_INT("Length of request list should be 0", app->request_list->count, 0);

    free(res);
    return error;
}

static int test_correct_id(FTP *app) {
    int error = 0;
    Response *res = mock_response();
    char packet[2000];
    int packet_index = mock_packet(packet, 1, 2);

    set_data_length(packet, 100);
    Request **req_container = &app->current_request; 
    process_pdu_header(packet, true, *res, req_container, app->request_list, app);
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


int protocol_handler_test() {
    int error = 0;
    error = test_process_pdu_header();
    error = test_process_pdu_eof();
    error = test_on_server_time_out();
    return error;
}