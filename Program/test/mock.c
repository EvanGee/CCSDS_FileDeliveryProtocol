
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

    add_new_cfdp_entity(mib, 3, 1, 1, csp, UN_ACKNOWLEDGED_MODE);   
    add_new_cfdp_entity(mib, 4, 2, 2, csp, UN_ACKNOWLEDGED_MODE);   

    add_new_cfdp_entity(mib, 5, 3, 3, csp, ACKNOWLEDGED_MODE);   
    add_new_cfdp_entity(mib, 6, 4, 4, csp, ACKNOWLEDGED_MODE);

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

Response mock_response() {
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


