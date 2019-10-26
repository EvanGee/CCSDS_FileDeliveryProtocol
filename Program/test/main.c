
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include "types.h"
#include "request_tests.h"
#include "packet_tests.h"


/*
#include "filesystem_tests.h"
#include "file_delivery_app.h"
#include "protocol_handler_tests.h"
#include "list_tests.h"
*/
#include "server_tests.h"



int main (int argc, char **argv) {
    

    int error = 0;

    error = request_tests();
    //error = protocol_handler_test();
    error = packet_tests();
    
    //error = list_tests();
    //error = file_system_tests();

    /*
    if (strcmp(argv[1], "1") == 0)
        error = server_tests(0);
    else 
        error = server_tests(1);
    */
   return error;
}