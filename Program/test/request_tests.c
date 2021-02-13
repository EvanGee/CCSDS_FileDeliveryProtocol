#include "protocol_handler.h"
#include "port.h"
#include "requests.h"
#include "string.h"
#include "test.h"
#include "filesystem_funcs.h"
#include "string.h"
#include "file_delivery_app.h"
#include "unit_tests.h"

static void list_print_id(Node *node, void *element, void *args) {
    Request *req = (Request *) element;
    printf("id: %d trans number: %llu\n", req->dest_cfdp_id, req->transaction_sequence_number);
}

//for finding the struct in the list
struct request_search_params {
    uint32_t source_id;
    uint32_t transaction_sequence_number;
};

//for finding the struct in the list
static int find_request(void *element, void *args) {
    Request *req = (Request *) element;
    struct request_search_params *params = (struct request_search_params *) args;
    if (req->dest_cfdp_id = params->source_id && req->transaction_sequence_number == params->transaction_sequence_number)
        return 1;
    return 0;
}

static void list_print(Node *node, void *element, void *args) {

    Request *req = (Request *) element;
    ssp_printf("%s\n", req->source_file_name);
}

static int request_finding_test() {

    List *list = linked_list();

    Request *req = mock_empty_request();
    req->dest_cfdp_id = 1;
    req->transaction_sequence_number = 1;
    list->push(list, req, req->dest_cfdp_id);

    Request *req2 = mock_empty_request();
    req2->dest_cfdp_id = 3;
    req2->transaction_sequence_number = 1;
    list->push(list, req2, req2->dest_cfdp_id);

    Request *req3 = mock_empty_request();
    req3->dest_cfdp_id = 2;
    req3->transaction_sequence_number = 2;
    list->push(list, req3, req3->dest_cfdp_id);


    struct request_search_params params = {
        req->dest_cfdp_id,
        req->transaction_sequence_number,
    };

    Request *found = list->find(list, 0, find_request, &params);
    params.source_id = 3;
    params.transaction_sequence_number = 1;
    Request *found2 = list->find(list, 0, find_request, &params);
    params.source_id = 2;
    params.transaction_sequence_number = 2;
    Request *found3 = list->find(list, 0, find_request, &params);

    if (found == NULL) {
        ssp_printf("CAN't FIND request IS NULL\n");
        list->free(list, ssp_cleanup_req);
        return 1;
    }
    
    ASSERT_EQUALS_INT("finding test, should equal", req->dest_cfdp_id, found->dest_cfdp_id);
    ASSERT_EQUALS_INT("finding test, should equal", req2->dest_cfdp_id, found2->dest_cfdp_id);
    ASSERT_EQUALS_INT("finding test, should equal", req3->dest_cfdp_id, found3->dest_cfdp_id);

    list->free(list, ssp_cleanup_req);
    return 0;
}


static void request_test_list_storage() {
    Request *req = mock_empty_request();
    List *list = linked_list();

    req->file = create_file("testfile.txt", 0);
    ssp_memcpy(req->source_file_name, "stuff", 6);
    list->insert(list, req, 1);

    Request *req2 = mock_empty_request();
    req2->file = create_file("testfile.txt", 0);
    ssp_memcpy(req2->source_file_name, "stuff2", 7);
    list->insert(list, req2, 2);

    Request *req3 = mock_empty_request();
    req3->file = create_file("testfile.txt", 0);
    ssp_memcpy(req3->source_file_name, "stuff3", 7);
    list->insert(list, req3, 3);

    ssp_cleanup_req(list->pop(list));
    list->iterate(list, list_print, NULL);  
    list->free(list, ssp_cleanup_req);
}

static int add_proxy_message() {

    Request *req = mock_empty_request();

    char *dest = "dest";
    char *src = "src";
    uint32_t id = 2;
    uint8_t len = 1;

    int error = add_proxy_message_to_request(id, len, src, dest, req);

    Message *message = req->messages_to_user->pop(req->messages_to_user);
    ASSERT_EQUALS_STR("message header should have asci: cfdp", message->header.message_id_cfdp, "cfdp", 5);

    Message_put_proxy *proxy = (Message_put_proxy *) message->value;
    ASSERT_EQUALS_STR("proxy dest_id should equal 2", proxy->destination_id.value, &id, len);
    ASSERT_EQUALS_STR("proxy src file", proxy->source_file_name.value, src,  proxy->source_file_name.length);
    ASSERT_EQUALS_STR("proxy dest file", proxy->destination_file_name.value, dest,  proxy->destination_file_name.length);

    ssp_free_message(message);
    ssp_cleanup_req(req);
    return 0;
}


int init_cont_partial_request_test_fail() {
    Message_cont_part_request p_cont;
    uint32_t dest_id = 0;
    uint32_t org_id = 1;
    uint64_t tran_id = 0;
    Request *req = mock_request();

    p_cont.destination_id.value = &dest_id;
    p_cont.originator_id.value = &org_id;
    p_cont.transaction_id.value = &tran_id;
    uint32_t buff_len = 1500;
    char buff[buff_len];

    int error = init_cont_partial_request(&p_cont, buff, buff_len);
    ASSERT_EQUALS_INT("failed to write, errored", error, -1);
    ssp_cleanup_req(req);
}

int init_cont_partial_request_test() {
    Message_cont_part_request p_cont;
    uint32_t dest_id = 3;
    uint32_t org_id = 2;
    uint64_t tran_id = 1;

    Request *req = mock_request();
    req->transaction_sequence_number = 1;
    req->dest_cfdp_id = 2;
    req->my_cfdp_id = 3;
    req->local_entity.EOF_recv_indication = 1;
    req->local_entity.Metadata_recv_indication = 1;

    save_req_to_file(req);

    p_cont.destination_id.value = &dest_id;
    p_cont.originator_id.value = &org_id;
    p_cont.transaction_id.value = &tran_id;
    uint32_t buff_len = 1500;
    char buff[buff_len];

    int error = init_cont_partial_request(&p_cont, buff, buff_len);

    Request *req2 = mock_empty_request();
    get_req_from_file(dest_id, tran_id, org_id, req2);
    ASSERT_EQUALS_INT("eof received, don't resend. local_entity.eof_recv = 1", 
    req->local_entity.EOF_recv_indication, 
    req2->local_entity.EOF_sent_indication);

    ASSERT_EQUALS_INT("metadata received, don't resend. local_entity.Metadata_sent = 1", 
    req->local_entity.Metadata_recv_indication, 
    req2->local_entity.Metadata_sent_indication);

    ssp_cleanup_req(req);
    ssp_cleanup_req(req2);
}

static int add_continue_partial_message_test() {
    int error = 0;
    Request *req = mock_empty_request();

    uint32_t src_id = 1;
    uint32_t dest_id = 1;
    uint32_t transaction_id = 4444;

    uint8_t len = 1;

    error = add_cont_partial_message_to_request(
        dest_id,
        len,
        src_id,
        len,
        transaction_id,
        4,
        req);

    Message *message = req->messages_to_user->pop(req->messages_to_user);
    ASSERT_EQUALS_STR("message header should have asci: cfdp", message->header.message_id_cfdp, "cfdp", 5);

    Message_cont_part_request *proxy = (Message_cont_part_request *) message->value;
    ASSERT_EQUALS_STR("proxy dest_id should equal 1", proxy->destination_id.value, &dest_id, proxy->destination_id.length);
    ASSERT_EQUALS_STR("proxy originator id should equal 1", proxy->originator_id.value, &src_id,  proxy->originator_id.length);
    ASSERT_EQUALS_STR("proxy transaction id should equal 4444", proxy->transaction_id.value, &transaction_id,  proxy->transaction_id.length);
    
    ssp_free_message(message);
    ssp_cleanup_req(req);

    return error;
}


int test_lv_functions() {

    char packet[100];
    
    char *str = "suphomie";
    LV lv; 
    create_lv(&lv, strnlen(str, 100), str);

    uint32_t len = strnlen(str, 100);

    ASSERT_EQUALS_INT("create_lv length works", lv.length, len);
    ASSERT_EQUALS_STR("create_lv value works", str, lv.value, len);

    uint16_t packet_index = copy_lv_to_buffer(packet, lv);
    ASSERT_EQUALS_INT("copy lv, length", packet[0], lv.length);
    ASSERT_EQUALS_STR("copy lv, value", &packet[1], lv.value, lv.length);

    free_lv(lv);

    copy_lv_from_buffer(&lv, packet, 0);
    ASSERT_EQUALS_INT("copy lv length from packet", lv.length, len);
    ASSERT_EQUALS_STR("copy lv value from packet", str, lv.value, len);
    free_lv(lv);
    
}


int request_user_input_tests() {

    FTP app;
    void *handler = create_ftp_task(1, &app);
    //need to let task initialize first.
    sleep(1);
    put_request(2, "", "", 0, &app);
    put_request(2, NULL, NULL, 0, &app);
    (&app)->close = true;
    ssp_thread_join(handler);
}
int scheduled_requests_test() {

    FTP app;
    void *handler = create_ftp_task(1, &app);
    int error = schedule_put_request(1, "test_files/dest.jpg", "test_files/scheduled_file_sent", ACKNOWLEDGED_MODE, &app);
    ASSERT_EQUALS_INT("couldn't schedule request when should have been able to ", error, 0);

    //error = schedule_put_request(1, "test_files/dest.jp", "test_files/scheduled_file_fail", ACKNOWLEDGED_MODE, app);
    //ASSERT_EQUALS_INT("couldn't schedule request, file not found", error, -1);

    //error = schedule_put_request(1, NULL, NULL, ACKNOWLEDGED_MODE, app);
    //ASSERT_EQUALS_INT("scheduling just messages", error, 0);
    
    sleep(1);
    app.close = true;
    ssp_thread_join(handler);
    return 0;
}

int schedule_requests_start_test(){
    sleep(1);
    
    FTP app;
    void *handler = create_ftp_task(1, &app);

    int error = start_scheduled_requests(1, &app);
    ASSERT_EQUALS_INT("start request batch successfully", error, 0);

    //app->close = true;
    ssp_thread_join(handler);
}

int test_process_messages() {

    //process_messages(Request *req, FTP *app) 
    return 0;
}

int request_tests() {

    int error = 0;
    
    
    error = request_finding_test(); 
    
    error = request_user_input_tests();
    /*
    error = add_proxy_message();
    error = test_lv_functions();
    error = add_continue_partial_message_test();
    error = init_cont_partial_request_test_fail();
    error = init_cont_partial_request_test();

    scheduled_requests_test();
    schedule_requests_start_test();
    */
    return error;
}