
#include "unit_tests.h"

int main () {
    
    int error = 0;
    
    //error = file_system_tests();
    //error = request_tests();
    //error = packet_tests();
    //error = protocol_handler_test();
    //error = tasks_tests();
    error = server_tests();
    //error = list_tests();
    
   return error;
}