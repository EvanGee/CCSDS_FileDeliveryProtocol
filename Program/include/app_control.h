/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef SSP_TASKS_H
#define SSP_TASKS_H

#include "types.h"

void ssp_cleanup_client(Client *client);
void ssp_cleanup_req(void *request);
void ssp_cleanup_ftp(FTP *app);
void *ssp_connectionless_server_task(void *params);
void *ssp_connectionless_client_task(void* params);

void *ssp_connection_client_task(void *params);
void *ssp_connection_server_task(void *params);
void *ssp_csp_connectionless_client_task(void *params);
void *ssp_csp_connectionless_server_task(void *params);
void *ssp_csp_connection_client_task(void *params);
void *ssp_csp_connection_server_task(void *params);
void ssp_client_join(Client *client);
void *ssp_generic_client_task(void *params);
void *ssp_generic_server_task(void *params);

void reset_timeout(int *prevtime);
void remove_request_check(Node *node, void *request, void *args);

#endif
