/*------------------------------------------------------------------------------
This file is protected under copyright. If you want to use it,
please include this text, that is my only stipulation.  

Author: Evan Giese
------------------------------------------------------------------------------*/
#ifndef MIB_H
#define MIB_H

#include "types.h"

int get_header_from_mib(Pdu_header *pdu_header, Remote_entity remote, uint32_t my_cfdp_id);
void ssp_cleanup_pdu_header(Pdu_header *pdu_header);
int get_remote_entity_from_json (Remote_entity *remote, uint32_t cfdp_id);

#endif
