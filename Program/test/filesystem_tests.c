#include "filesystem_funcs.h"
#include "unit_tests.h"
#include "port.h"
#include "mib.h"
#include "requests.h"
#include <fcntl.h>


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

int test_save_file() {

    char *file_name = "temp/test_file_save";
    int fd = ssp_open(file_name, O_RDWR | O_CREAT);
    if (fd < 0) {
        ssp_error("couldn't open file\n");
        return -1;
    }
    int *error = 0;
    File file;
    file.missing_offsets = linked_list();

    Offset one = {
        0, 250
    };
    Offset two = {
        0, 250
    };
    Offset three = {
        0, 250
    };
    
    file.missing_offsets->push(file.missing_offsets, &one, -1);
    file.missing_offsets->push(file.missing_offsets, &two, -1);
    file.missing_offsets->push(file.missing_offsets, &three, -1);

    save_file_meta_data(fd, error, &file);

}

static int test_saving_request(){

    int error = 0;

    Request *req = init_request(1000);
    req->dest_cfdp_id = 1;
    req->transaction_sequence_number = 1;
    req->local_entity.EOF_recv_indication = 1;
    req->local_entity.EOF_sent_indication = 1;
    req->local_entity.suspended_indication = 1;
    char *dest_file = "stuff";
    char *src_file = "morestuff";

    Message *m = create_message(PROXY_PUT_REQUEST);
    m->value = create_message_put_proxy(1, 1, dest_file, src_file);
    req->messages_to_user->push(req->messages_to_user, m, 0);

    m = create_message(PROXY_PUT_REQUEST);
    m->value = create_message_put_proxy(1, 1, dest_file, src_file);
    req->messages_to_user->push(req->messages_to_user, m, 0);

    m = create_message(PROXY_PUT_REQUEST);
    m->value = create_message_put_proxy(1, 1, dest_file, src_file);
    req->messages_to_user->push(req->messages_to_user, m, 0);

    printf("write test\n");
    error = save_req(req);
    if (error == -1)
        printf("failed to write\n");
    
    printf("read test\n");
    Request *got_req = get_req(1, 1);
    
    print_request_state(got_req);
    
    ssp_cleanup_req(req);
    ssp_cleanup_req(got_req);
    return error;
}


int file_system_tests() {
    int error = 0;
    //int error = test_saving_request();
    error = test_save_file();
    return error;
}

