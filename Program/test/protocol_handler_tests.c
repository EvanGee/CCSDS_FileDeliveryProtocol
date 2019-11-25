

#include "protocol_handler.h"
#include "stdlib.h"
#include "mib.h"
#include "test.h"
#include "file_delivery_app.h"
#include "unit_tests.h"


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
static int test_wrong_id(FTP *app) {

    Response res = mock_response();
    char *packet = build_mock_packet(app, 2);
    Request **req_container = &app->current_request; 
    process_pdu_header(packet, true, res, req_container, app->request_list, app);
    ASSERT_EQUALS_INT("Test wrong id, Request should be NULL", (*req_container), NULL);
    free(packet);
}

static int test_correct_id(FTP *app) {

    Response res = mock_response();
    char *packet = build_mock_packet(app, 1);
    Request **req_container = &app->current_request; 
    process_pdu_header(packet, true, res, req_container, app->request_list, app);
    ASSERT_NOT_EQUALS_INT("Test wrong id, Request should not be NULL", (*req_container), NULL);
    ASSERT_EQUALS_INT("Length of request should be 1", app->request_list->count, 1);

    free(packet);
}

static int test_process_pdu_header() {


    FTP *app = init_ftp(1);
    
    
    test_wrong_id(app);
    test_correct_id(app);
    /*
    Request **req_container = &app->current_request; 
    
    Response res = mock_response();

    //test 1
    
    process_pdu_header(packet, true, res, req_container, app->request_list, app);

    if (*req_container != NULL) {
        ASSERT_EQUALS_INT("request transaction number should equal", (*req_container)->transaction_sequence_number, 1);
        ASSERT_EQUALS_INT("souce id should equal", (*req_container)->dest_cfdp_id, 2);
    }
    //test 2
    //char *packet2 = build_mock_packet(app, 3);
    //process_pdu_header(packet2, 1, res, req_container, app->request_list, app);
    //ASSERT_EQUALS_INT("request transaction number should equal", (*req_container)->transaction_sequence_number, 1);
    //ASSERT_NOT_EQUALS_INT("souce id should not equal", (*req_container)->dest_cfdp_id, 2);
    //ASSERT_EQUALS_INT("souce id should equal", (*req_container)->dest_cfdp_id, 3);

    */
    //free(packet2);
    
    app->close = true;
    ssp_thread_join(app->server_handle);
}


int protocol_handler_test() {
    int error = 0;
    error = test_process_pdu_header();
    return error;
}