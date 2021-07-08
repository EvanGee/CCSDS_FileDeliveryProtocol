/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef UTILS_H
#define UTILS_H

#include "stdint.h"

void ssp_print_hex(char *stuff, int size);
void ssp_print_bits(char *stuff, int size);
char *safe_strncpy(char *to, char*from, int len);

#endif //UTILS_H
