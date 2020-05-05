#ifndef SSP_UNIT_TEST_H
#define SSP_UNIT_TEST_H
    #include "types.h"
    #include "test.h"

    List *populate_request_list();


    //int tasks_tests();    
    //int request_tests();
    //int test_header(char *packet, Pdu_header *header, uint64_t sequence_number);
    int packet_tests();
    int protocol_handler_test();
    

    int mock_packet(char *packet, uint32_t dest_id, uint32_t src_id);
    Response *mock_response();
File *mock_eof_packet(char *packet, uint32_t dest_id, uint32_t src_id, char *file_name);
    Request *mock_request();
    int file_system_tests();
    int request_tests();
    
    #define TEMP_FILESIZE 1000

    //int server_tests(int client   );

#endif