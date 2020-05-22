
/*------------------------------------------------------------------------------
CMPT-361-AS50(1) - 2017 Fall - Introduction to Networks
Assignment #2
Evan Giese 1689223

This is my utils.c file, it contains useful function and abstract data types
to use for general functionality. 
------------------------------------------------------------------------------*/

#include "port.h"
#include "utils.h"
//size is the number of bytes we want to print
void ssp_print_hex(char *stuff, int size){
    
    uint32_t current_packet_index = 0;
    ssp_printf("printing number of bytes: %u\n", size);

        for (int j = 0; j < size; j += 1) {
            ssp_printf("%x.", 
            stuff[current_packet_index]);
            current_packet_index += 1; 
        }
        ssp_printf("\n");
}   
