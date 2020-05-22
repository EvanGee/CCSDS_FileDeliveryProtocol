/*------------------------------------------------------------------------------
CMPT-361-AS50(1) - 2017 Fall - Introduction to Networks
Assignment #1
Evan Giese 1689223

This file is the header file for utils.c
------------------------------------------------------------------------------*/
#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

//https://stackoverflow.com/questions/3022552/is-there-any-standard-htonl-like-function-for-64-bits-integers-in-c
#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

void ssp_print_hex(char *stuff, int size);


#endif //UTILS_H
