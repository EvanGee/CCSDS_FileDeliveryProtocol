#include "unit_tests.h"

int main () {
    
    int error = 0;
    
    error = request_tests();
    error = packet_tests();
    error = protocol_handler_test();
    error = list_tests();
    error = tasks_tests();
    error = file_system_tests();

   return error;
}