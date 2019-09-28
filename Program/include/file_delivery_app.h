


#ifndef FTP_APP_H
#define FTP_APP_H
#include "types.h"

Protocol_state  *init_ftp(uint32_t my_cfdp_address);

Client *ssp_client(uint32_t cfdp_id, Protocol_state *p_state);
void ssp_server(Protocol_state *p_state);


#endif