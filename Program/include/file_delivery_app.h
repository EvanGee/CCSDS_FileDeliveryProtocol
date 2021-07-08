/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef FTP_APP_H
#define FTP_APP_H
#include "types.h"

int init_ftp(uint32_t my_cfdp_address, FTP *app);
Client *ssp_client(uint32_t cfdp_id, FTP *app);
void create_ssp_server(FTP *app);
int create_ssp_server_drivers(FTP *app);
void *create_ftp_task(uint32_t cfdp_id, FTP *app);
Client *init_client(uint32_t dest_cfdp_id, uint32_t my_cfdp_id);

#endif
