/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef UTILS_H
#define UTILS_H

#include "stdint.h"

//https://stackoverflow.com/questions/3022552/is-there-any-standard-htonl-like-function-for-64-bits-integers-in-c
#define htonll(x) ((1==ssp_htonl(1)) ? (x) : ((uint64_t)ssp_htonl((x) & 0xFFFFFFFF) << 32) | ssp_htonl((x) >> 32))
#define ntohll(x) ((1==ssp_ntohl(1)) ? (x) : ((uint64_t)ssp_ntohl((x) & 0xFFFFFFFF) << 32) | ssp_ntohl((x) >> 32))

void ssp_print_hex(char *stuff, int size);


#endif //UTILS_H
