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
    receive_offset(file, 0, 5, 50);
    
    file->missing_offsets->iterate(file->missing_offsets, nak_print, 0);
    receive_offset(file, 0, 100, 1000);

    file->missing_offsets->iterate(file->missing_offsets, nak_print, 0);
    receive_offset(file, 0, 50, 100);

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


int test_write_lv() {

    DECLARE_NEW_TEST("test write lv");
    char *my_file_name = "temp/test_lv_write";
    int fd = ssp_open(my_file_name, O_RDWR | O_CREAT);
    if (fd < 0) {
        ssp_error("couldn't open file\n");
        return -1;
    }

    LV lv;
    lv.value = my_file_name;
    lv.length = strnlen(my_file_name, 255);
    
    int error = write_lv(fd, lv);
    if (error < 0) {
        return -1;
    }
    //seek back to start of file
    ssp_lseek(fd, 0, 0);

    LV new_lv;
    error = read_lv(fd, &new_lv);
    if (error < 0) {
        return -1;
    }

    ASSERT_EQUALS_STR("write&read lv value to file", lv.value, my_file_name, lv.length);
    ASSERT_EQUALS_INT("write&read lv length", new_lv.length, lv.length);
    free_lv(new_lv);

    return 1;
}

int test_save_file() {


    DECLARE_NEW_TEST("test saving file");

    char *file_name = "temp/test_file_save";
    int fd = ssp_open(file_name, O_RDWR | O_CREAT);
    if (fd < 0) {
        ssp_error("couldn't open file\n");
        return -1;
    }
    int *error = 0;
    File file;
    memset(&file, 0, sizeof(File));

    file.missing_offsets = linked_list();

    Offset one = {
        0, 250
    };
    Offset two = {
        100, 250
    };
    Offset three = {
        250, 250
    };
    
    file.missing_offsets->push(file.missing_offsets, &one, -1);
    file.missing_offsets->push(file.missing_offsets, &two, -1);
    file.missing_offsets->push(file.missing_offsets, &three, -1);


    save_file_to_file(fd, &file);

    File read;
    memset(&read, 0, sizeof(File));

    file.missing_offsets->freeOnlyList(file.missing_offsets);

    ssp_lseek(fd, 0, 0);

    get_file_from_file(fd, &read);

    Offset *offset = read.missing_offsets->pop(read.missing_offsets);
    ASSERT_EQUALS_INT("offset start should equal", three.start, offset->start);
    ASSERT_EQUALS_INT("offset end should equal", three.end, offset->end);
    ssp_free(offset);

    offset = read.missing_offsets->pop(read.missing_offsets);
    ASSERT_EQUALS_INT("offset start should equal", two.start, offset->start);
    ASSERT_EQUALS_INT("offset end should equal", two.end, offset->end);
    ssp_free(offset);
    
    offset = read.missing_offsets->pop(read.missing_offsets);
    ASSERT_EQUALS_INT("offset start should equal", one.start, offset->start);
    ASSERT_EQUALS_INT("offset end should equal", one.end, offset->end);
    ssp_free(offset);

    ASSERT_EQUALS_INT("File eof_checksum", file.eof_checksum, read.eof_checksum);
    ASSERT_EQUALS_INT("File fd --should be ignored", file.fd, read.fd);
    
    ASSERT_EQUALS_INT("File is_temp", file.is_temp, read.is_temp);
    ASSERT_EQUALS_INT("File next_offset_to_send", file.next_offset_to_send, read.next_offset_to_send);

    ASSERT_EQUALS_INT("File partial_checksum", file.partial_checksum, read.partial_checksum);
    ASSERT_EQUALS_INT("File total_size", file.total_size, read.total_size);

    read.missing_offsets->free(read.missing_offsets, ssp_free);
}

static int test_saving_request(){

    int error = 0;

    DECLARE_NEW_TEST("test saving requests");

    Request *req = mock_empty_request();

    req->dest_cfdp_id = 1;
    req->transaction_sequence_number = 1;
    req->my_cfdp_id = 1;

    req->local_entity.EOF_recv_indication = 1;
    req->local_entity.EOF_sent_indication = 1;
    req->local_entity.suspended_indication = 1;
    char *dest_file = "stuff";
    char *src_file = "morestuff";

    Message *m = create_message(PROXY_PUT_REQUEST);
    m->value = create_message_put_proxy(1, 1, src_file, dest_file);
    req->messages_to_user->push(req->messages_to_user, m, 0);

    Message *m2 = create_message(PROXY_PUT_REQUEST);
    m2->value = create_message_put_proxy(1, 1, src_file, dest_file);
    req->messages_to_user->push(req->messages_to_user, m2, 0);

    Message *m3 = create_message(PROXY_PUT_REQUEST);
    m3->value = create_message_put_proxy(1, 1, src_file, dest_file);
    req->messages_to_user->push(req->messages_to_user, m3, 0);

    error = save_req_to_file(req);
    if (error == -1)
        printf("failed to write\n");

    ssp_cleanup_req(req);
}

int test_get_file(){
    DECLARE_NEW_TEST("test getting requests");
    Request *req = mock_empty_request();
    char *dest_file = "stuff";
    char *src_file = "morestuff";

    int error = get_req_from_file(1, 1, 1, req);
    if (error < 0)
        printf("failed to read\n");
    
    Message *message = req->messages_to_user->pop(req->messages_to_user);
    Message_put_proxy *proxy_message = (Message_put_proxy *) message->value;
    
    ASSERT_EQUALS_STR("third message src file name", proxy_message->destination_file_name.value, dest_file, proxy_message->destination_file_name.length);
    ASSERT_EQUALS_STR("third message src file name", proxy_message->source_file_name.value, src_file, proxy_message->source_file_name.length);
    ASSERT_EQUALS_INT("third message id", proxy_message->destination_id, req->dest_cfdp_id);
    ssp_free_message(message);

    message = req->messages_to_user->pop(req->messages_to_user);
    proxy_message = (Message_put_proxy *) message->value;
    ASSERT_EQUALS_STR("second message src file name", proxy_message->destination_file_name.value, dest_file, sizeof(dest_file));
    ASSERT_EQUALS_STR("second message src file name", proxy_message->source_file_name.value, src_file, sizeof(src_file));
    ASSERT_EQUALS_INT("second message id", proxy_message->destination_id, req->dest_cfdp_id);
    ssp_free_message(message);

    message = req->messages_to_user->pop(req->messages_to_user);
    proxy_message = (Message_put_proxy *) message->value;
    ASSERT_EQUALS_STR("first message src file name", proxy_message->destination_file_name.value, dest_file, sizeof(dest_file));
    ASSERT_EQUALS_STR("first message src file name", proxy_message->source_file_name.value, src_file, sizeof(src_file));
    ASSERT_EQUALS_INT("first message id", proxy_message->destination_id, req->dest_cfdp_id);
    ssp_free_message(message);

    ssp_cleanup_req(req);
    //need to free list
    return error;
}

int test_save_file_and_messages() {

    DECLARE_NEW_TEST("test saving file and messages");
    int error = 0;
    Request *req = mock_empty_request();

    req->dest_cfdp_id = 2;
    req->transaction_sequence_number = 2;
    req->my_cfdp_id = 2;
    req->local_entity.EOF_recv_indication = 1;
    req->local_entity.EOF_sent_indication = 1;
    req->local_entity.suspended_indication = 1;
    req->file = NULL;
    char *dest_file = "stuff";
    char *src_file = "morestuff";

    Message *m = create_message(PROXY_PUT_REQUEST);
    m->value = create_message_put_proxy(1, 1, src_file, dest_file);
    req->messages_to_user->push(req->messages_to_user, m, 0);

    m = create_message(PROXY_PUT_REQUEST);
    m->value = create_message_put_proxy(1, 1, src_file, dest_file);
    req->messages_to_user->push(req->messages_to_user, m, 0);

    m = create_message(PROXY_PUT_REQUEST);
    m->value = create_message_put_proxy(1, 1, src_file, dest_file);
    req->messages_to_user->push(req->messages_to_user, m, 0);

    error = save_req_to_file(req);
    if (error == -1)
        printf("failed to write\n");


    File *file = ssp_alloc(1, sizeof(File));
    file->missing_offsets = linked_list();


    
    Offset *one = ssp_alloc(1, sizeof(Offset));
    Offset *two = ssp_alloc(1, sizeof(Offset));
    Offset *three = ssp_alloc(1, sizeof(Offset));

    one->start = 0;
    one->end = 250;
    two->start = 100;
    two->end = 250;
    three->start = 250;
    three->end = 250;
    
    file->missing_offsets->push(file->missing_offsets, one, -1);
    file->missing_offsets->push(file->missing_offsets, two, -1);
    file->missing_offsets->push(file->missing_offsets, three, -1);
    req->file = file;
    error = save_req_to_file(req);
    if (error < 0)
        printf("failed to write\n");
    
    Request *got_req = mock_empty_request();

    error = get_req_from_file(2, 2, 2, got_req);
    if (error < 0)
        printf("failed to write\n");

    print_request_state(got_req);
    ssp_cleanup_req(req);
    ssp_cleanup_req(got_req);

    return 1;
}





int file_system_tests() {
    int error = 0;
    error = test_write_lv();
    error = test_saving_request();
    error = test_save_file();
    error = test_get_file();
    error = test_save_file_and_messages();
    return error;
}

