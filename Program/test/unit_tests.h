#ifndef SSP_UNIT_TEST_H
#define SSP_UNIT_TEST_H
    #include "types.h"
    #include "test.h"

    List *populate_request_list();


    int tasks_tests();    
    int request_tests();
    int test_header(char *packet, Pdu_header *header, uint64_t sequence_number);
    int packet_tests();
    int protocol_handler_test();
    

    #define TEMP_FILESIZE 1000
    int file_system_tests();

    //int server_tests(int client   );

#endif