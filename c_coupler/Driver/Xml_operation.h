#ifndef XML_OPERA_H
#define XML_OPERA_H

#include <string.h>
#include "compset_communicators_info_mgt.h"
#include "inout_interface_mgt.h"
#include "memory_mgt.h"
#include "coupling_generator.h"

//files path

extern void write_components_into_xml(Comp_comm_group_mgt_mgr*);
extern void write_interfaces_into_xml(Inout_interface_mgt*,Comp_comm_group_mgt_node *);
extern void write_fields_into_xml(Memory_mgt*);
extern void write_Coupling_connections_into_xml(Coupling_generator*);
//extern Comp_comm_group_mgt_global_node* read_xml(const char*);

#endif
