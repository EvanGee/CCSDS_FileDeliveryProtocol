//the management information base (MIB)

//default faul handle


#ifndef MIB_H
#define MIB_H

#include "types.h"

int get_header_from_mib(Pdu_header *pdu_header, Remote_entity remote, uint32_t my_cfdp_id);

void ssp_cleanup_pdu_header(Pdu_header *pdu_header);
Remote_entity *get_remote_entity(MIB *mib, uint32_t dest_id);
Remote_entity *get_remote_entity2(uint32_t dest_id);
int get_remote_entity_from_json (Remote_entity *remote, uint32_t cfdp_id);

#endif