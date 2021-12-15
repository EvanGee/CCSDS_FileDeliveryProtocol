/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef REQUESTS_H
#define REQUESTS_H
#include "types.h"


//--------------------------------------------User functions--------------------------------
/*
params:  
    id of destination,  
    source file name,  
    name of the file as it will arrive at destination,  
    ACKNOWLEDGED_MODE/UN_ACKNOWLEDGED_MODE (ACKNOWLEDGED_MODE will allow for acks/naks to be sent.),  
    app from create_ftp_task
*/

Request *put_request(
            uint32_t dest_id,
            char *source_file_name,
            char *destination_file_name,
            uint8_t transmission_mode,
            FTP *app
            );

/*
params:  
    id of destination,  
    source file name,  
    name of the file as it will arrive at destination,  
    ACKNOWLEDGED_MODE/UN_ACKNOWLEDGED_MODE (ACKNOWLEDGED_MODE will allow for acks/naks to be sent.),  
    app from create_ftp_task
*/
Request *get_request(
            uint32_t dest_id,
            char *source_file_name,
            char *destination_file_name,
            uint8_t transmission_mode,
            FTP *app);

//to start sending packets
void start_request(Request *req);

/*
params:  
    id of destination,  
    source file name,  
    name of the file as it will arrive at destination,  
    ACKNOWLEDGED_MODE/UN_ACKNOWLEDGED_MODE (ACKNOWLEDGED_MODE will allow for acks/naks to be sent.),  
    app from create_ftp_task
*/
int add_proxy_message_to_request(uint32_t beneficial_cfid, uint8_t length_of_id, char *source_name, char *dest_name, Request *req);

//doesn't really work yet (have to fix byte order for storate)
int add_cont_partial_message_to_request(uint32_t beneficial_cfid, 
                                    uint32_t originator_id,
                                    uint32_t transaction_id,
                                    Request *req);


//-----------------------------------------------------------------------------------------

void ssp_cleanup_req(void *request);
Request *init_request(char *buff, uint32_t buff_len);
Message_put_proxy *create_message_put_proxy(uint32_t beneficial_cfid, uint8_t length_of_id, char *source_name, char *dest_name);
Message_cont_part_request *create_message_cont_partial_request(uint32_t beneficial_cfid, 
                                                    uint32_t originator_id,
                                                    uint32_t transaction_id);

int init_cont_partial_request(Message_cont_part_request *p_cont, char *buff, uint32_t buff_len);
void print_request_state(Request *req);
void ssp_free_message(void *params);
uint16_t copy_lv_to_buffer(char *buffer, LV lv);
void copy_lv_from_buffer(LV *lv, char *packet, uint32_t start);
Message *create_message(uint8_t type);
void create_lv(LV *lv, int len, void *value);
void free_lv(LV lv);
void print_request_procedure(Request *req);
int start_scheduled_requests(uint32_t dest_id, FTP *app);
int schedule_put_request(uint32_t dest_id,char *source_file_name,char *destination_file_name,uint8_t transmission_mode, FTP *app);
int schedule_request(Request *req, uint32_t dest_id, FTP *app);

Request *init_request_no_client(void);

int put_request_no_client(
    Request *req,
    char *source_file_name,
    char *destination_file_name,
    uint8_t transmission_mode,
    FTP *app);

void print_res(Response res);
void add_request_to_client(Request *req, Client *client);
Client *start_client(FTP *app, uint8_t dest_id);
#endif
