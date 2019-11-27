

#include "protocol_handler.h"
#include "stdlib.h"
#include "mib.h"
#include "test.h"
#include "file_delivery_app.h"
#include "unit_tests.h"
#include "port.h"

static char *build_mock_packet(FTP *app, uint32_t id) {

    char *packet = calloc(sizeof(char*), 2000);
    Pdu_header *pdu_header = get_header_from_mib(app->mib, id, app->my_cfdp_id);
    build_pdu_header(packet, 1, 0, pdu_header);
    ssp_cleanup_pdu_header(pdu_header);
    return packet;
}

static Response mock_response() {
    Response res;
    int addr = 16;
    res.addr = &addr;
    res.sfd = 1;
    res.packet_len = 2000;
    res.size_of_addr = 16;
    return res;
}

static int test_process_pdu_eof() {
//char *packet, Request *req, Response res

    
    //process_pdu_eof(packet, Request *req, Response res);
    return 0;
}



static int test_wrong_id(FTP *app) {
    int error = 0;
    Response res = mock_response();
    char *packet = build_mock_packet(app, 2);
    Request **req_container = &app->current_request; 
    process_pdu_header(packet, true, res, req_container, app->request_list, app);
    Request *req = (*req_container);
    error = ASSERT_NULL("Test wrong id, Request should be NULL", req);
    free(packet);
    return error;
}

static int test_correct_id(FTP *app) {
    int error = 0;
    Response res = mock_response();
    char *packet = build_mock_packet(app, 1);
    Request **req_container = &app->current_request; 
    process_pdu_header(packet, true, res, req_container, app->request_list, app);
    Request *req = (*req_container);
    error = ASSERT_NOT_NULL("Test wrong id, Request should not be NULL", req);
    error = ASSERT_EQUALS_INT("Length of request should be 1", app->request_list->count, 1);
    error = ASSERT_EQUALS_INT("request transaction number should equal", req->transaction_sequence_number, 1);
    error = ASSERT_NOT_EQUALS_INT("souce id should equal 2", req->dest_cfdp_id, 2);

    free(packet);
    return error;

}

static int test_process_pdu_header() {

    int error = 0;
    FTP *app = init_ftp(1);

    error = test_wrong_id(app);
    error = test_correct_id(app);

    app->close = true;
    ssp_thread_join(app->server_handle);
    return error;
}


int protocol_handler_test() {
    int error = 0;
    error = test_process_pdu_header();
    error = test_process_pdu_eof();
    return error;
}