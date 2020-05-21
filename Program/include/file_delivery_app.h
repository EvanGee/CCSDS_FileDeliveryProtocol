


#ifndef FTP_APP_H
#define FTP_APP_H
#include "types.h"

FTP *init_ftp(uint32_t my_cfdp_address);
Client *ssp_client(uint32_t cfdp_id, FTP *app);
void create_ssp_server(FTP *app);

#endif