#include "filesystem_funcs.h"
#include "unit_tests.h"
#include "port.h"
#include "mib.h"
#include "requests.h"

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


int file_system_tests() {
    int error = 0;

    Request *req = init_request(1000);
    req->dest_cfdp_id = 1;
    req->transaction_sequence_number = 1;
    req->local_entity.EOF_recv_indication = 1;
    req->local_entity.EOF_sent_indication = 1;
    req->local_entity.suspended_indication = 1;
    
    req->messages_to_user->push(req->messages_to_user, create_message(PROXY_PUT_REQUEST), 0);

    printf("write test\n");
    error = save_req_json(req);
    if (error == -1)
        printf("failed to write\n");

    error = get_req_json(1, 1);
    if (error == -1)
        return -1;
    
    //ssp_cleanup_req(req);
    return error;
}

