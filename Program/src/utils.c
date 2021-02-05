/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#include "port.h"
#include "utils.h"
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

void ssp_print_bits(char *stuff, int size){
    
    uint32_t current_packet_index = 0;
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