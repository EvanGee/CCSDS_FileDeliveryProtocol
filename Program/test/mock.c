
#include "types.h"
#include "filesystem_funcs.h"
#include "port.h"
#include "protocol_handler.h"
#include "mib.h"
#include "file_delivery_app.h"
#include "packet.h"
#include "unit_tests.h"
#include "requests.h"

#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>



MIB *mock_mib() {

    //Memory information base
    MIB *mib = init_mib();

    //setting host name for testing
    char *host_name = "127.0.0.1";
    uint32_t addr[sizeof(uint32_t)];
    inet_pton(AF_INET, host_name, addr);
    
    //adding new cfdp entities to management information base
    add_new_cfdp_entity(mib, 1, *addr, 1111, posix, UN_ACKNOWLEDGED_MODE);
    add_new_cfdp_entity(mib, 2, *addr, 1112, posix, UN_ACKNOWLEDGED_MODE); 
    add_new_cfdp_entity(mib, 7, *addr, 1113, posix, UN_ACKNOWLEDGED_MODE); 

    //add_new_cfdp_entity(mib, 3, 1, 1, csp, UN_ACKNOWLEDGED_MODE);   
    //add_new_cfdp_entity(mib, 4, 2, 2, csp, UN_ACKNOWLEDGED_MODE);   

    //add_new_cfdp_entity(mib, 5, 3, 3, csp, ACKNOWLEDGED_MODE);   
    //add_new_cfdp_entity(mib, 6, 4, 4, csp, ACKNOWLEDGED_MODE);

    return mib;
}


int mock_packet(char *packet, uint32_t dest_id, uint32_t src_id) {

    MIB *mib = mock_mib();
    Pdu_header *pdu_header = get_header_from_mib(mib, dest_id, src_id);
    int packet_index = build_pdu_header(packet, 1, 0, pdu_header);

    ssp_cleanup_pdu_header(pdu_header);
    free_mib(mib);
    
    return packet_index;
}

void mock_eof_packet(char *packet, uint32_t dest_id, uint32_t src_id) {
    
    int packet_index = mock_packet(packet, dest_id, src_id);
    File *file = create_file("dest.jpg", false);
    file->partial_checksum = check_sum_file(file, 1000);
    build_eof_packet(packet, packet_index, file);
    free_file(file);

}



Response *mock_response() {
    Response *res = calloc(1, sizeof(Response));
    int addr = 16;
    res->addr = &addr;
    res->sfd = 1;
    res->packet_len = 2000;
    res->size_of_addr = 16;
    res->type_of_network = posix;
    res->transmission_mode = UN_ACKNOWLEDGED_MODE;
    return res;
}




Request *mock_request() {
    Request *req = init_request(5000);
    MIB *mib = mock_mib();

    char *dest = "dest";
    char *src = "src";
    uint32_t id = 1;

    req->dest_cfdp_id = id;
    req->file = create_file("dest_received.jpg", true);
    memcpy (req->source_file_name, dest, strnlen(dest, 255)); 
    memcpy (req->destination_file_name, src, strnlen(src, 255));

    req->pdu_header = get_header_from_mib(mib, id, 2);
    req->remote_entity = get_remote_entity(mib, 1);

    int addr = 16;
    req->res.addr = malloc(5);
    memcpy(req->res.addr, &addr, 4);
    req->res.sfd = 1;
    req->res.packet_len = 2000;
    req->res.size_of_addr = 16;
    req->res.type_of_network = posix;
    req->res.transmission_mode = UN_ACKNOWLEDGED_MODE;
    req->res.msg = req->buff;

    free_mib(mib);

    return req;
}   


/*
static int test_process_pdu_eof() {
//char *packet, Request *req, Response res

    
    //process_pdu_eof(packet, Request *req, Response res);
    return 0;
}


*/

