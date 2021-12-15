/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#include "port.h"
#include "utils.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

//size is the number of bytes we want to print
void ssp_print_hex(char *stuff, int size){
    
    uint32_t current_packet_index = 0;
    ssp_printf("printing number of bytes: %u\n", size);
        int j = 0;
        for (j = 0; j < size; j += 1) {
            ssp_printf("%x.", 
            stuff[current_packet_index]);
            current_packet_index += 1; 
        }
        ssp_printf("\n");
}   


void log_ftp(char *info, char *stuff){

    #ifdef POSIX_PORT
    static int log_fd = -1;
    time_t current_time;
    char c_time_string[1000];
    current_time = time(NULL);

    if (current_time == -1) {
        ssp_printf("Failure to obtain the current time.\n");
    }
    
    struct tm *time = localtime(&current_time);

    ssp_snprintf(c_time_string, sizeof(c_time_string), "%d-%d-%dT%d:%d:%dZ|%s|%s", 
    time->tm_year, 
    time->tm_mon, 
    time->tm_mday, 
    time->tm_hour, 
    time->tm_min, 
    time->tm_sec,
    info,
    stuff
    );

    if (c_time_string == NULL) {
        printf("Failure to obtain the current time string.\n");
        return;
    }
    
    if (log_fd < 0) {

        log_fd = ssp_open("log.txt", SSP_O_RDWR, 0655);
        if (log_fd == -1){
            log_fd = ssp_open("log.txt", SSP_O_CREAT | SSP_O_RDWR, 0655);
        }
        else {
            log_fd = ssp_open("log.txt", SSP_O_RDWR | O_APPEND);
           
        }

        int size = strnlen(c_time_string, sizeof(c_time_string));
        if (size < 0) {
            printf("Failure to obtain the current time string.\n");
            return;
        }

        int bytes = write(log_fd, c_time_string, size);
        if (bytes < 0) {
            printf("Failure to write log string.\n");
            return;
        }

        close(log_fd);
    }
    #endif
    return;
}

void ssp_print_bits(char *stuff, int size){
    
    ssp_printf("printing number of bytes: %u\n", size);
    int j, i, bit_set, byte = 0;
    unsigned char bit_mask = 0;

    for (i = 0; i < size; i++) {
        byte = stuff[i];
        bit_mask = 128;
        for (j = 7; j >= 0 ; j--) {
            bit_set = bit_mask & byte;
            bit_mask = bit_mask >> 1;

            if (bit_set){
                ssp_printf("1");
            } else {
                ssp_printf("0");
            }
        }
        ssp_printf(" ");
    }
    ssp_printf("\n");
}


//replace strings with this 

char *safe_strncpy(char *to, char*from, int len){
    void *ret = strncpy(to, from, len);
    to[len] = '\0';
    return ret;
}
