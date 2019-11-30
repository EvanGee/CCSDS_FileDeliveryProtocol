
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include "types.h"
#include "unit_tests.h"



int main () {
    

    int error = 0;

    //error = request_tests();
    error = packet_tests();
    //error = protocol_handler_test();
    
    //error = list_tests();
    //error = file_system_tests();
    //error = tasks_tests();
    
    /*
    if (strcmp(argv[1], "1") == 0)
        error = server_tests(0);
    else 
        error = server_tests(1);
    */
   return error;
}