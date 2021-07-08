
/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef PACKET_H
#define PACKET_H

//dont change this
#define PACKET_STATIC_HEADER_LEN 4 //in bytes

#include "types.h"
#include "list.h"

int build_pdu_header(char *packet, uint64_t transaction_sequence_number, uint32_t transmission_mode, uint16_t data_len, Pdu_header *pdu_header);
uint8_t build_finished_pdu(char *packet, uint32_t start);
uint8_t build_put_packet_metadata(char *packet, uint32_t start, Request *req);
uint8_t build_nak_response(char *packet, uint32_t start, uint32_t offset, Request *req, Client* client);
void get_finished_pdu(char *packet, Pdu_finished *pdu_finished);
int get_nak_packet(char *packet, Pdu_nak *nak);

int copy_id_to_packet(char *bytes, uint64_t id);
int copy_id_lv_to_packet(char *bytes, uint64_t id);
int copy_id_lv_from_packet(char *bytes,  uint64_t *id);
uint64_t copy_id_from_packet(char *bytes, uint32_t length_of_ids);
void set_packet_directive(char *packet, uint32_t location, uint8_t directive);
void set_bits_to_protocol_byte(char *byte, uint8_t from_position, uint8_t to_position, uint8_t value);


uint32_t get_data_offset_from_packet(char *packet);

int build_data_packet(char *packet, uint32_t start, uint32_t end, uint32_t offset, File *file);
void build_eof_packet(char *packet, uint32_t start, uint32_t file_size, uint32_t checksum);
void get_eof_from_packet(char *packet, Pdu_eof *eof);

void fill_nak_array_callback(Node *node, void *element, void *args);
uint32_t build_nak_packet(char *packet, uint32_t start, Request *req);
uint8_t build_ack(char*packet, uint32_t start, uint8_t type);
uint8_t build_nak_directive(char *packet, uint32_t start, uint8_t directive) ;
void set_data_length(char*packet, uint16_t data_len);
uint16_t get_data_length(char*packet);
int get_pdu_header_from_packet(char *packet, Pdu_header *pdu_header);
uint8_t get_bits_from_protocol_byte(char byte, uint8_t from_position, uint8_t to_position);
void ssp_print_header(Pdu_header *pdu_header);
uint32_t get_message_from_packet(char *packet, uint32_t start, Request *req);
uint32_t get_messages_from_packet(char *packet, uint32_t start, uint32_t data_length, Request *req);
uint32_t add_messages_to_packet(char *packet, uint32_t start, List *messages_to_user);
void get_ack_from_packet(char *packet, Pdu_ack *ack);


#endif
