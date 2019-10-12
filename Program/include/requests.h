
#ifndef REQUESTS_H
#define REQUESTS_H

#include "stdint.h"
#include "protocol_handler.h"
#include "types.h"

void ssp_cleanup_req(void *request);
Request *init_request(uint32_t buff_len);

Request *put_request(
            uint32_t dest_id,
            char *source_file_name,
            char *destination_file_name,
            uint8_t transmission_mode,
            FTP *app
            );

            
#endif