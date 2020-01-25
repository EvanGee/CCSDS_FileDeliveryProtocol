
#include "mib.h"
#include "utils.h"
#include "port.h"
#include <stddef.h>
#include "string.h"
#include "list.h"
#include "types.h"
#include "filesystem_funcs.h"
#include "stdlib.h"

//snprintf
#include "stdio.h"


int get_header_from_mib2 (Pdu_header *pdu_header, Remote_entity remote, uint32_t my_cfdp_id) {

    pdu_header->reserved_bit_0 = 0;
    pdu_header->reserved_bit_1 = 0;
    pdu_header->reserved_bit_2 = 0;
    pdu_header->CRC_flag = remote.CRC_required;
    pdu_header->direction = 1;
    pdu_header->PDU_type = 0;
    pdu_header->transaction_seq_num_len = 3;
    pdu_header->length_of_entity_IDs = 1; 
    pdu_header->transmission_mode = remote.default_transmission_mode;
    pdu_header->destination_id = remote.cfdp_id;
    pdu_header->source_id = my_cfdp_id;

    return 0;
}


enum {
    PARSE_cfdp_id,
    PARSE_UT_address,
    PARSE_UT_port,
    PARSE_type_of_network,
    PARSE_default_transmission_mode,
    PARSE_one_way_light_time,
    PARSE_total_round_trip_allowance,
    PARSE_async_NAK_interval,
    PARSE_async_keep_alive_interval,
    PARSE_async_report_interval,
    PARSE_immediate_nak_mode_enabled,
    PARSE_prompt_transmission_interval,
    PARSE_disposition_of_incomplete,
    PARSE_CRC_required,
    PARSE_max_file_segment_len,
    PARSE_keep_alive_discrepancy_limit,
    PARSE_positive_ack_timer_expiration_limit,
    PARSE_nak_timer_expiration_limit,
    PARSE_transaction_inactivity_limit,
    PARSE_TOTAL,
};

static char *parse_list[PARSE_TOTAL] = {   
    "cfdp_id" ,
    "UT_address",
    "UT_port" ,
    "type_of_network",  
    "default_transmission_mode" ,

    "one_way_light_time" ,
    "total_round_trip_allowance" ,
    "async_NAK_interval",
    "async_keep_alive_interval",
    "async_report_interval" ,
    "immediate_nak_mode_enabled" ,
    "prompt_transmission_interval" ,
    "disposition_of_incomplete" ,
    
    "CRC_required" ,
    "max_file_segment_len" ,
    "keep_alive_discrepancy_limit" ,
    "positive_ack_timer_expiration_limit" ,
    "nak_timer_expiration_limit" ,
    "transaction_inactivity_limit" 
};


static void parse_mib(char *key, char *value, void *params) {

    int len = 0;
    for (int i = 0; i < PARSE_TOTAL; i++) {
        len = strnlen(parse_list[i], 50);
        
        if (strncmp(key, parse_list[i], len) != 0)
            continue;

        Remote_entity *remote = (Remote_entity *) params;

        switch (i)
        {
            case PARSE_cfdp_id: 
                remote->cfdp_id = atol(value);
                break;
            case PARSE_UT_address: 
                remote->UT_address = atol(value);
                break;
            case PARSE_UT_port: 
                remote->UT_port = atol(value);
                break;
            case PARSE_type_of_network: 
                remote->type_of_network = atol(value);
                break;
            case PARSE_default_transmission_mode:
                remote->default_transmission_mode = atol(value); 
                break;
            case PARSE_one_way_light_time: 
                remote->one_way_light_time = atol(value);
                break;
            case PARSE_total_round_trip_allowance:
                remote->total_round_trip_allowance = atol(value); 
                break;
            case PARSE_async_NAK_interval: 
                remote->async_NAK_interval = atol(value);
                break;
            case PARSE_async_keep_alive_interval:
                remote->async_keep_alive_interval = atol(value); 
                break;
            case PARSE_async_report_interval:
                remote->async_report_interval = atol(value); 
                break;
            case PARSE_immediate_nak_mode_enabled:
                remote->immediate_nak_mode_enabled = atol(value); 
                break;
            case PARSE_prompt_transmission_interval:
                remote->prompt_transmission_interval = atol(value); 
                break;
            case PARSE_disposition_of_incomplete:
                remote->disposition_of_incomplete = atol(value); 
                break;
            case PARSE_CRC_required: 
                remote->CRC_required = atol(value);
                break;
            case PARSE_max_file_segment_len: 
                remote->max_file_segment_len = atol(value);
                break;
            case PARSE_keep_alive_discrepancy_limit:
                remote->keep_alive_discrepancy_limit = atol(value); 
                break;
            case PARSE_positive_ack_timer_expiration_limit: 
                remote->positive_ack_timer_expiration_limit = atol(value);
                break;
            case PARSE_nak_timer_expiration_limit:
                remote->nak_timer_expiration_limit = atol(value); 
                break;
            case PARSE_transaction_inactivity_limit:
                remote->transaction_inactivity_limit = atol(value); 
                break;
            default:
                break;
        }
            
        
    }
  
}

int get_remote_entity_from_json (Remote_entity *remote, uint32_t cfdp_id) {

    char file_name[50];
    snprintf(file_name, 50, "%s%d%s", "mib/peer_", cfdp_id, ".json");
    
    int error = read_json(file_name, parse_mib, remote);

    if (error < 0) {
        ssp_error("could not get remote, json parsing failed\n");
        return -1;
    }

    return 0;

}


void ssp_cleanup_pdu_header(Pdu_header *pdu_header) {
    ssp_free(pdu_header);
}






