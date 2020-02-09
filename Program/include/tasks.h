
#ifndef SSP_TASKS_H
#define SSP_TASKS_H

#include "types.h"
#include "list.h"

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

void remove_request_check(Node *node, void *request, void *args);

#endif