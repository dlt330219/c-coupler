#include <mpi.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include "Xml_operation.h"
#include "tinyxml.h"
#include "tinystr.h"
#include "global_data.h"
#include "common_utils.h"
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

using namespace std;

TiXmlDocument *init_xml()
{
    TiXmlDocument *XML_file;
    TiXmlDeclaration *XML_declaration;

    XML_file = new TiXmlDocument;
    XML_declaration = new TiXmlDeclaration(("1.0"),(""),(""));
    EXECUTION_REPORT(REPORT_ERROR, -1, XML_file != NULL, "Software error: cannot create an xml file");
    
    XML_file->LinkEndChild(XML_declaration);
    return XML_file;

}

void write_comp_node_into_xml(TiXmlElement *parent_Element,Comp_comm_group_mgt_node *node)
{
	if(node == NULL)
    {
        cout<<"node is NULL"<<endl;
		return;
    }

    int i, num_segments;
    int *segments_start, *segments_end;
    char *string;
	
	TiXmlElement * current_Element = new TiXmlElement("Online_Model");
	
    parent_Element->LinkEndChild(current_Element);
	current_Element->SetAttribute("global_id",node->get_unified_global_id());
	current_Element->SetAttribute("name",node->get_comp_name());
	current_Element->SetAttribute("full_name",node->get_comp_full_name());

    segments_start = new int [node->get_num_procs()];
    segments_end = new int [node->get_num_procs()];
    segments_start[0] = node->get_local_proc_global_id(0);
    for (i =1, num_segments =1; i < node->get_num_procs(); i ++){
        if (node->get_local_proc_global_id(i) != node->get_local_proc_global_id(i-1)+1) {
            segments_end[num_segments-1] = node->get_local_proc_global_id(i-1);
            segments_start[num_segments] = node->get_local_proc_global_id(i);
            num_segments ++;
        }
    }
    segments_end[num_segments-1] = node->get_local_proc_global_id(node->get_num_procs()-1);
    string = new char [num_segments*(8*2+1)];
    string[0] = '\0';
    for (i = 0; i < num_segments; i ++) {
        if (segments_start[i] != segments_end[i])
            sprintf(string+strlen(string), " %d~%d", segments_start[i], segments_end[i]);
        else sprintf(string+strlen(string), " %d", segments_start[i]);
    }
     
    current_Element->SetAttribute("processes",string);

    delete [] segments_start;
    delete [] segments_end;
    delete [] string;

	parent_Element = current_Element;

	for (int i = 0;i<node->get_num_children();i++)
	{
		Comp_comm_group_mgt_node *tmp= node->get_child(i);
		write_comp_node_into_xml(parent_Element,tmp);
	}		
}
/*
void write_import_or_export_interfaces_into_xml(TiXmlElement *import_or_export_interfaces_element, std::vector<Inout_interface*> interfaces)
{
    for(int i = 0; i < interfaces.size(); i ++)
    {
	TiXmlElement *interface_element;
	interface_element = new TiXmlElement("interface");
	import_or_export_interfaces_element->LinkEndChild(interface_element);
	interface_element->SetAttribute("name",interfaces[i]->get_interface_name());
	interface_element->SetAttribute("id",interfaces[i]->get_interface_id());
	
	TiXmlElement *fields_element = new TiXmlElement("fields");
	interface_element->LinkEndChild(fields_element);

	std::vector<Field_mem_info*> fields_mem_registered;
	interfaces[i]->get_fields_mem_registered(fields_mem_registered);

	for (int j = 0; j < fields_mem_registered.size(); j ++)
	{
	    TiXmlElement *field_element = new TiXmlElement("field");
	    interface_element->LinkEndChild(field_element);
	    field_element->SetAttribute("name",fields_mem_registered[j]->get_field_name()); 
	    field_element->SetAttribute("field_id",fields_mem_registered[j]->get_field_instance_id());
	}
	TiXmlElement *timers_element = new TiXmlElement("timers");
	interface_element->LinkEndChild(timers_element);
	std::vector<Coupling_timer*> timers;
	interfaces[i]->get_timers(timers);
	for (int j = 0; j < timers.size(); j++)
	{
	    TiXmlElement *timer_element = new TiXmlElement("timer");
	    timers_element->LinkEndChild(timer_element);
	    timer_element->SetAttribute("frequency_count",timers[j]->get_frequency_count());
	    timer_element->SetAttribute("frequency_unit",timers[j]->get_frequency_unit());
	    timer_element->SetAttribute("delay_count",timers[j]->get_lag_count());
	}
    }
}
void write_interfaces_into_xml(Inout_interface_mgt *inout_interface_mgt, Comp_comm_group_mgt_node *node)
{
    if (node->get_current_proc_local_id() == 0)
    if (node != comp_comm_group_mgt_mgr->get_global_node_root())
    {
	    TiXmlDocument *XML_file = init_xml();
	    TiXmlElement *root_element = new TiXmlElement("Component");
	    XML_file->LinkEndChild(root_element);
	    char XML_file_name[NAME_STR_SIZE];
	    sprintf(XML_file_name, "%s/CCPL_configs/XML/%s_%d_interfaces.xml",comp_comm_group_mgt_mgr->get_root_working_dir(),node->get_comp_name(), node->get_unified_global_id());

	    std::vector<Inout_interface*> import_interfaces;
	    std::vector<Inout_interface*> export_interfaces;

	    root_element->SetAttribute("component_id",node->get_unified_global_id());

	    inout_interface_mgt->get_all_import_interfaces_of_a_component(import_interfaces,node->get_comp_id());
	    inout_interface_mgt->get_all_export_interfaces_of_a_component(export_interfaces,node->get_comp_id());

	    TiXmlElement *import_interfaces_element = new TiXmlElement("import_interfaces");
            TiXmlElement *export_interfaces_element = new TiXmlElement("export_interfaces");

	    root_element->LinkEndChild(import_interfaces_element);
	    write_import_or_export_interfaces_into_xml(import_interfaces_element,import_interfaces);

	    root_element->LinkEndChild(export_interfaces_element);
	    write_import_or_export_interfaces_into_xml(export_interfaces_element,export_interfaces);

	    XML_file->SaveFile(XML_file_name);
    }

    for (int i = 0; i < node->get_num_children(); i++)
    {
	Comp_comm_group_mgt_node *tmp= node->get_child(i);
	if (tmp != NULL) 
	write_interfaces_into_xml(inout_interface_mgt,tmp);
    }
}
*/

void write_components_into_xml(Comp_comm_group_mgt_mgr *comp_comm_group_mgt_mgr)
{
    TiXmlDocument *XML_file = init_xml();
    TiXmlElement *root_element;
    Comp_comm_group_mgt_node * root = comp_comm_group_mgt_mgr->get_global_node_root();
    char XML_file_name[NAME_STR_SIZE];

    if (comp_comm_group_mgt_mgr->get_current_proc_global_id() != 0)
        return;

    root_element = new TiXmlElement("Components");
    XML_file->LinkEndChild(root_element);
    write_comp_node_into_xml(root_element,root);
    sprintf(XML_file_name, "%s/CCPL_configs/XML/components.xml",comp_comm_group_mgt_mgr->get_root_working_dir());

    XML_file->SaveFile(XML_file_name);

}

bool search_element_by_attribute(TiXmlElement *parent_Element, TiXmlElement *element,char *attribute_name, char *attribute_value )
{
    TiXmlNode *child;
    element = parent_Element;
    for (child = parent_Element->FirstChild(); child != NULL; child = child->NextSibling())
    {
		TiXmlElement *temp = child->ToElement();
		if (strcmp(temp->Attribute(attribute_name), attribute_value) == 0){
			element = temp;
			return true;
		}
    }
    return false;
}

void write_fields_into_xml(Memory_mgt *memory_manager)
{
    TiXmlDocument *XML_file = init_xml(); 
    TiXmlElement *root_element;
    Comp_comm_group_mgt_node *root = comp_comm_group_mgt_mgr->get_global_node_root();
    char XML_file_name[NAME_STR_SIZE];

    if (comp_comm_group_mgt_mgr->get_current_proc_global_id() == 0)
    {
    	root_element = new TiXmlElement("fields");
    	XML_file->LinkEndChild(root_element);
	
	//memory_manager->get_fields_mem(fields_mem);
    	for(int i = 0; i < memory_manager->get_num_fields(); i ++)
    	{
		Field_mem_info *field_mem = memory_manager->get_field_instance(i);
		char *comp_id;
		sprintf(comp_id, "%d", field_mem->get_comp_id());
		TiXmlElement *field_parent_element;
		search_element_by_attribute(root_element,field_parent_element,"comp_id",comp_id);

		if (field_parent_element == root_element)
		{
		    TiXmlElement *comp_element = new TiXmlElement("component");
	     	root_or_comp_element->LinkEndChild(comp_element);
		    comp_element->SetAttribute("comp_id",field_mem->get_comp_id());
		    field_parent_element = comp_element;
		}

		TiXmlElement *field_element;
		if(search_element_by_attribute(field_parent_element,field_element,"field_name",field_mem->get_field_name())){
			continue;
		}
		field_element = new TiXmlElement("field");
		field_parent_element->LinkEndChild(field_element);
		field_element->SetAttribute("field_name", field_mem->get_field_name());
		//field_element->SetAttribute("field_id",field_mem->get_field_instance_id());
		field_element->SetAttribute("grid_id",field_mem->get_grid_id());
		field_element->SetAttribute("decomp_id",field_mem->get_decomp_id());
		field_element->SetAttribute("field_unit",field_mem->get_unit());
   	}

    sprintf(XML_file_name, "%s/CCPL_configs/XML/fields.xml", comp_comm_group_mgt_mgr->get_root_working_dir());
    XML_file->SaveFile(XML_file_name);
    }
}

void write_coupling_connections_into_xml(Coupling_generator *coupling_generator)
{

} 

/*
bool write_xml(Comp_comm_group_mgt_global_node *node)
{
	TiXmlDocument *pDoc = new TiXmlDocument;
	if(pDoc == NULL)
	{
		return false;
	}
	
	TiXmlDeclaration *pDeclaration = new TiXmlDeclaration(("1.0"),(""),(""));
	if(pDeclaration == NULL)
	{
		return false;
	}

	pDoc->LinkEndChild(pDeclaration);
	TiXmlElement * rootElement = new TiXmlElement("Components");
	pDoc->LinkEndChild(rootElement);
	
	Comp_comm_group_mgt_global_node *root = node;
	
    const char *path = COMP_PATH + root.get_comp_full_name() + ".xml";
    
	while(root->get_parent()!= NULL)
	{
		root = node->get_parent();
	}
	
	TiXmlElement * element = NULL;
    //cout<<"travel the tree"<<endl;
	write_node_into_XML(rootElement,root);

    pDoc->SaveFile(path);
}
*/



/*
Comp_comm_group_mgt_global_node* travel_read(TiXmlElement *pElement,Comp_comm_group_mgt_global_node* node,bool is_Root)
{
	if(!pElement)
		cout<<"NULL"<<endl;
	Comp_comm_group_mgt_global_node* temp;
	if(is_Root)
	{
		//cout<<atoi(pElement->Attribute("global_id"))<<endl;
		node = new Comp_comm_group_mgt_global_node(pElement->Attribute("name"),pElement->Attribute("type"),atoi(pElement->Attribute("global_id")));
		int* process_ids = node->processes_ids_char_to_int(pElement->Attribute("processes"));
		node->set_processes_ids(process_ids);
		is_Root = false;
	}
	else
	{
		temp = new Comp_comm_group_mgt_global_node(pElement->Attribute("name"),pElement->Attribute("type"),atoi(pElement->Attribute("global_id")));
		int* process_ids = temp->processes_ids_char_to_int(pElement->Attribute("processes"));
		temp->set_processes_ids(process_ids);
		temp->set_parent(node);
		node->set_child(temp);
		node = temp;
	}
	TiXmlNode* first_child = pElement->FirstChild();
	if(first_child)
	{
		travel_read(first_child->ToElement(),node,is_Root);
		while(first_child->NextSibling())
		{
			TiXmlNode* element = first_child->NextSibling();
			travel_read(element->ToElement(),node,is_Root);
		}
	}
	return node;
}
Comp_comm_group_mgt_global_node* read_xml(const char* path)
{
	TiXmlDocument dom(path);
	bool succ = dom.LoadFile();
	if(!succ)
	{
		cout<<"open fail"<<endl;
		return NULL;
	}
	TiXmlElement *pElement = dom.FirstChildElement();
	TiXmlElement *Online_Model = pElement->FirstChildElement();
	//cout<<pElement->Attribute("name")<<endl;
	bool is_Root = true;
	Comp_comm_group_mgt_global_node* root;
	root = travel_read(Online_Model,root,is_Root);
	//cout<<root->get_comp_name()<<endl;
	return root;
}
*/
