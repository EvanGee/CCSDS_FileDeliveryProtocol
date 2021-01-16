#ifndef SSP_UNIT_TEST_H
#define SSP_UNIT_TEST_H
#include "types.h"
#include "test.h"



int tasks_tests();    
int packet_tests();
int protocol_handler_test();
int list_tests();
int file_system_tests();
int request_tests();
int protocol_handler_test();

int mock_packet(char *packet, uint32_t dest_id, uint32_t src_id);
Response *mock_response();
File *mock_eof_packet(char *packet, uint32_t dest_id, uint32_t src_id, char *file_name);
Request *mock_request();
Request *mock_empty_request();
List *populate_request_list();

#define TEMP_FILESIZE 1000

//int server_tests(int client   );

#endif