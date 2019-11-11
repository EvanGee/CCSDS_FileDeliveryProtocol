
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


//timeout_max is how long we want to check, time is our start, returned from start_timer
int check_timeout(int *prevtime, uint32_t timeout) {

    int prev = *prevtime;
    int current_time = ssp_time_count();
    int time_out = prev + timeout;

    if (current_time >= time_out) {
        *prevtime = current_time;
        printf("current time %d  timeout target %d\n", current_time, time_out);
        return 1;
    }
    else if (current_time < prev) {
        *prevtime = current_time;
        printf("wrap arround\n");
    }
    return 0; 
}

int main (int argc, char **argv) {
    

    int error = 0;

    //error = request_tests();
    //error = protocol_handler_test();
    //error = packet_tests();
    //error = list_tests();
    //error = file_system_tests();
    //error = tasks_tests();
    uint32_t seconds = 0;
    int prevtime = 0;

    bool is_timeout = false;

    while (1) {
        is_timeout = check_timeout(&prevtime, 100);
        
        if (is_timeout)
            printf("timeout! %d\n", is_timeout);
        
    }
    /*
    if (strcmp(argv[1], "1") == 0)
        error = server_tests(0);
    else 
        error = server_tests(1);
    */
   return error;
}