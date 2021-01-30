/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef FTP_APP_H
#define FTP_APP_H
#include "types.h"

FTP *init_ftp(uint32_t my_cfdp_address);
Client *ssp_client(uint32_t cfdp_id, FTP *app);
void create_ssp_server(FTP *app);
int create_ssp_server_drivers(FTP *app);
int create_ftp_task();

#endif
