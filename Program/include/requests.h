
#ifndef REQUESTS_H
#define REQUESTS_H

#include "stdint.h"
#include "protocol_handler.h"
#include "types.h"

void ssp_cleanup_req(void *request);
Request *init_request(uint32_t buff_len);
void start_request(Request *req);

Request *put_request(
            uint32_t dest_id,
            char *source_file_name,
            char *destination_file_name,
            uint8_t transmission_mode,
            FTP *app
            );

int add_proxy_message_to_request(uint32_t beneficial_cfid, uint8_t length_of_id, char *source_name, char *dest_name, Request *req);

Message_put_proxy *create_message_put_proxy(uint32_t beneficial_cfid, uint8_t length_of_id, char *source_name, char *dest_name, Request *req);

void print_request_state(Request *req);
//Frees a message struct
void ssp_free_message(void *params);
uint16_t copy_lv_to_buffer(char *buffer, LV lv);
void copy_lv_from_buffer(LV *lv, char *packet, uint32_t start);
Message *create_message(uint8_t type);
void create_lv(LV *lv, int len, void *value);
void free_lv(LV lv);
void print_request_procedure(Request *req);

#endif