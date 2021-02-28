/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef PROTOCOL_H
#define PROTOCOL_H
#include "types.h"

#define RESEND_EOF_TIMES 3
#define RESEND_FINISHED_TIMES 10
#define TIMEOUT_BEFORE_CANCEL_REQUEST 1200
#define TIMEOUT_BEFORE_SAVE_REQUEST 30

int parse_packet_server(char *packet, uint32_t packet_index, Response res, Request *req, Pdu_header incoming_header, FTP *app);
void user_request_handler(Response res, Request *req, Client *client);
void parse_packet_client(char* buff, uint32_t packet_index, Response res, Request *req, Client *client);
void on_server_time_out(Response res, Request *current_request);
int process_pdu_header(char*packet, uint8_t is_server, Pdu_header *incoming_pdu_header, Response res, Request **req, List *request_list, FTP *app);

void process_pdu_eof(char *packet, Request *req, Response res);

 Request *new_incomming_request(uint32_t source_id, 
        uint32_t transmission_mode, 
        uint32_t transaction_sequence_number,
        Response res,
        FTP *app);

uint32_t parse_metadata_packet(char *meta_data_packet, uint32_t start, Request *req_to_fill);
void process_messages(Request *req, FTP *app);
void process_data_packet(char *packet, uint32_t data_len, File *file);
int create_data_burst_packets(char *packet, uint32_t start, File *file, uint32_t length);


uint8_t build_ack (char *packet, uint32_t start, uint8_t type);
int process_file_request_metadata(Request *req);
int nak_response(char *packet, uint32_t start, Request *req, Response res, Client *client);
void set_data_length(char*packet, uint16_t data_len);
uint32_t build_nak_packet(char *packet, uint32_t start, Request *req);
uint16_t get_data_length(char*packet);



#endif
