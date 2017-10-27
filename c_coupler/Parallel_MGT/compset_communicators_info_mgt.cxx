/***************************************************************
  *  Copyright (c) 2017, Tsinghua University.
  *  This is a source file of C-Coupler.
  *  This file was initially finished by Dr. Li Liu. 
  *  If you have any problem, 
  *  please contact Dr. Li Liu via liuli-cess@tsinghua.edu.cn
  ***************************************************************/


#include "compset_communicators_info_mgt.h"
#include <stdio.h>
#include <string.h>
#include "global_data.h"
#include "cor_global_data.h"
#include "quick_sort.h"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>



void recursively_remove_directory()
{
    DIR *cur_dir = opendir(".");
    struct dirent *ent = NULL;
    struct stat st;
 
    if (cur_dir == NULL)
        return;
 
    while ((ent = readdir(cur_dir)) != NULL) {
        stat(ent->d_name, &st);
     
        if (words_are_the_same(ent->d_name, ".") || words_are_the_same(ent->d_name, ".."))
            continue;
 
        if (S_ISDIR(st.st_mode)) {
            chdir(ent->d_name);
            recursively_remove_directory();
            chdir("..");
        }
        remove(ent->d_name);
    }
     
    closedir(cur_dir);
}


void remove_directory(const char *path)
{
    char old_path[NAME_STR_SIZE];
 
    getcwd(old_path, NAME_STR_SIZE);
     
    if (chdir(path) == -1)
    	EXECUTION_REPORT(REPORT_ERROR, -1, false, "Fail to open \"%s\" which should be a directory");
	
    recursively_remove_directory();
    chdir(old_path);
}


void create_directory(const char *path, MPI_Comm comm, bool is_root_proc, bool new_dir)
{
	char buffer[NAME_STR_SIZE];
	

	if (is_root_proc) {
		DIR *dir=opendir(path);
		if (dir != NULL && new_dir)
			remove_directory(path);
		if (dir == NULL) {
			umask(0);
			mkdir(path, S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
			int retcode = errno;
			dir=opendir(path);
			EXECUTION_REPORT(REPORT_ERROR, -1, dir != NULL, "Directory \"%s\" cannot be created: %s. Please verify.", path, strerror(retcode));
		}
	}	
}


Comp_comm_group_mgt_node::~Comp_comm_group_mgt_node()
{
	if (temp_array_buffer != NULL)
		delete [] temp_array_buffer;

	if (restart_mgr != NULL)
		delete restart_mgr;

	if (proc_latest_model_time != NULL)
		delete [] proc_latest_model_time;
}


Comp_comm_group_mgt_node::Comp_comm_group_mgt_node(Comp_comm_group_mgt_node *buffer_node, Comp_comm_group_mgt_node *parent, int &global_node_id)
{	
	int num_procs, *proc_id, num_children;


	int old_iter = buffer_content_iter;
	read_data_from_array_buffer(annotation_end, NAME_STR_SIZE, buffer_node->temp_array_buffer, buffer_node->buffer_content_iter, true);
	read_data_from_array_buffer(annotation_start, NAME_STR_SIZE, buffer_node->temp_array_buffer, buffer_node->buffer_content_iter, true);
	read_data_from_array_buffer(working_dir, NAME_STR_SIZE, buffer_node->temp_array_buffer, buffer_node->buffer_content_iter, true);
	read_data_from_array_buffer(exe_log_file_name, NAME_STR_SIZE, buffer_node->temp_array_buffer, buffer_node->buffer_content_iter, true);
	read_data_from_array_buffer(comp_log_file_name, NAME_STR_SIZE, buffer_node->temp_array_buffer, buffer_node->buffer_content_iter, true);
	read_data_from_array_buffer(full_name, NAME_STR_SIZE, buffer_node->temp_array_buffer, buffer_node->buffer_content_iter, true);
	read_data_from_array_buffer(comp_name, NAME_STR_SIZE, buffer_node->temp_array_buffer, buffer_node->buffer_content_iter, true);
	read_data_from_array_buffer(comp_type, NAME_STR_SIZE, buffer_node->temp_array_buffer, buffer_node->buffer_content_iter, true);
	read_data_from_array_buffer(&comm_group, sizeof(MPI_Comm), buffer_node->temp_array_buffer, buffer_node->buffer_content_iter, true);
	read_data_from_array_buffer(&num_procs, sizeof(int), buffer_node->temp_array_buffer, buffer_node->buffer_content_iter, true);
	EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Comm_rank(MPI_COMM_WORLD, &current_proc_global_id) == MPI_SUCCESS);
	proc_id = new int [num_procs];
	for (int i = 0; i < num_procs; i ++)
		read_data_from_array_buffer(&proc_id[num_procs-1-i], sizeof(int), buffer_node->temp_array_buffer, buffer_node->buffer_content_iter, true);
	for (int i = 0; i < num_procs; i ++)
		local_processes_global_ids.push_back(proc_id[i]);
	this->comp_id = -1;
	this->comm_group = -1;
	this->current_proc_local_id = -1;
	this->working_dir[0] = '\0';
	this->comp_log_file_name[0] = '\0';
	this->exe_log_file_name[0] = '\0';
	this->proc_latest_model_time = NULL;
	global_node_id ++;
	this->parent = parent;
	temp_array_buffer = NULL;
	definition_finalized = true;
	unified_global_id = 0;
	restart_mgr = new Restart_mgt(comp_id);

	read_data_from_array_buffer(&num_children, sizeof(int), buffer_node->temp_array_buffer, buffer_node->buffer_content_iter, true);
	for (int i = 0; i < num_children; i ++)
		children.push_back(new Comp_comm_group_mgt_node(buffer_node, this, global_node_id));
}


Comp_comm_group_mgt_node::Comp_comm_group_mgt_node(const char *comp_name, const char *comp_type, int comp_id, Comp_comm_group_mgt_node *parent, MPI_Comm &comm, const char *annotation)
{
	std::vector<char*> unique_comp_name;
	int i, j, num_procs, current_proc_local_id_in_parent, *process_comp_id, *processes_global_id;
	MPI_Comm parent_comm;
	char dir[NAME_STR_SIZE];
	Comp_comm_group_mgt_node *ancestor = parent;

	
	strcpy(this->comp_name, comp_name);
	strcpy(this->comp_type, comp_type);
	if (parent != NULL && words_are_the_same(parent->get_comp_type(), COMP_TYPE_PSEUDO_COUPLED) && parent->children.size() > 0)
		EXECUTION_REPORT(REPORT_ERROR, parent->get_comp_id(), false, "Error happens when registering the component model \"%s\": its parent \"%s\" is an inactive component model and already has one child \"%s\". Please note that an inactive component model can have at most one child. Please check the model code with the annotation \"%s\"", comp_name, parent->comp_name, parent->children[0]->comp_name, annotation);

	while(ancestor != NULL && words_are_the_same(ancestor->get_comp_type(), COMP_TYPE_PSEUDO_COUPLED)) 
		ancestor = ancestor->get_parent();
	if (ancestor == NULL || words_are_the_same(ancestor->get_comp_name(), "ROOT"))
		strcpy(this->full_name, this->comp_name);
	else sprintf(this->full_name, "%s@%s", ancestor->get_full_name(), this->comp_name);
	strcpy(this->annotation_start, annotation);
	this->annotation_end[0] = '\0';
	this->comp_id = comp_id;
	this->parent = parent;
	this->buffer_content_size = 0;
	this->buffer_max_size = 1024;
	this->temp_array_buffer = new char [buffer_max_size];
	this->definition_finalized = false;	
	this->unified_global_id = 0;
	this->proc_latest_model_time = NULL;
	restart_mgr = new Restart_mgt(comp_id);

	if (comm != -1) {
		comm_group = comm;
		if (parent == NULL)
			synchronize_comp_processes_for_API(-1, API_ID_COMP_MGT_REG_COMP, comm, "checking the given communicator for registering root component", annotation);
		else {
			char tmp_string[NAME_STR_SIZE];
			sprintf(tmp_string, "for checking the given communicator for registering a child component \"%s\"", comp_name);
			synchronize_comp_processes_for_API(parent->get_comp_id(), API_ID_COMP_MGT_REG_COMP, comm, tmp_string, annotation);			
			check_API_parameter_string(parent->get_comp_id(), API_ID_COMP_MGT_REG_COMP, comm, "registering a component model", parent->get_comp_name(), "\"parent_id\" (the corresponding component model)", annotation);
		}	
	}
	else {
		EXECUTION_REPORT(REPORT_ERROR,-1, parent != NULL, "Software error in Comp_comm_group_mgt_node::Comp_comm_group_mgt_node for checking parent");
		synchronize_comp_processes_for_API(parent->get_comp_id(), API_ID_COMP_MGT_REG_COMP, parent->get_comm_group(), "for checking the communicator of the current component for registering its children component", annotation);
		check_API_parameter_string(parent->get_comp_id(), API_ID_COMP_MGT_REG_COMP, parent->get_comm_group(), "registering a component model", parent->get_comp_name(), "\"parent_id\" (the corresponding component model)", annotation);
		parent_comm = parent->get_comm_group();
		if ((parent->comp_id&TYPE_ID_SUFFIX_MASK) != 0)
			EXECUTION_REPORT_LOG(REPORT_LOG, parent->comp_id, true, 
			                 "Before the MPI_barrier for synchronizing all processes of the parent component \"%s\" for registerring its children components including \"%s\" (the corresponding model code annotation is \"%s\")", 
			                 parent->get_comp_name(), comp_name, annotation);
		else if (parent->get_current_proc_local_id() == 0) 
			EXECUTION_REPORT_LOG(REPORT_LOG, -1, true, 
			                 "Before the MPI_barrier for synchronizing all processes of the whole coupled model for registerring root components including \"%s\" (the corresponding model code annotation is \"%s\")", 
			                 comp_name, annotation);	
		EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Barrier(parent_comm) == MPI_SUCCESS);
		if ((parent->comp_id&TYPE_ID_SUFFIX_MASK) != 0)
			EXECUTION_REPORT_LOG(REPORT_LOG, parent->comp_id, true, 
			                 "After the MPI_barrier for synchronizing all processes of the parent component \"%s\" for registerring its children components including \"%s\" (the corresponding model code annotation is \"%s\")", 
			                 parent->get_comp_name(), comp_name, annotation);
		else if (parent->get_current_proc_local_id() == 0) 
			EXECUTION_REPORT_LOG(REPORT_LOG, -1, true, 
			                 "After the MPI_barrier for synchronizing all processes of the whole coupled model for registerring root components including \"%s\" (the corresponding model code annotation is \"%s\")", 
			                 comp_name, annotation);	
		EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Comm_size(parent_comm, &num_procs) == MPI_SUCCESS);
		EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Comm_rank(parent_comm, &current_proc_local_id_in_parent) == MPI_SUCCESS);
		char *all_comp_name;
		if (current_proc_local_id_in_parent == 0) 
			all_comp_name = new char [NAME_STR_SIZE*num_procs];
		process_comp_id = new int [num_procs];
		EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Gather((char*)comp_name, NAME_STR_SIZE, MPI_CHAR, all_comp_name, NAME_STR_SIZE, MPI_CHAR, 0, parent_comm) == MPI_SUCCESS);
		if (current_proc_local_id_in_parent == 0) {
			unique_comp_name.push_back(all_comp_name);
			process_comp_id[0] = 0;
			for (i = 1; i < num_procs; i ++) {
				for (j = 0; j < unique_comp_name.size(); j ++)
					if (words_are_the_same(unique_comp_name[j], all_comp_name+i*NAME_STR_SIZE)) {
						break;
					}
				if (j == unique_comp_name.size())
					unique_comp_name.push_back(all_comp_name+i*NAME_STR_SIZE);
				process_comp_id[i] = j;
			}
		}
		EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Bcast(process_comp_id, num_procs, MPI_INT, 0, parent_comm)  == MPI_SUCCESS);
		EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Comm_split(parent_comm, process_comp_id[current_proc_local_id_in_parent], 0, &comm_group) == MPI_SUCCESS);
		if (current_proc_local_id_in_parent == 0)
			delete [] all_comp_name;
		delete [] process_comp_id;
		comm = comm_group;
	}

	EXECUTION_REPORT(REPORT_ERROR, -1, words_are_the_same(comp_type, COMP_TYPE_CPL) || words_are_the_same(comp_type, COMP_TYPE_ATM) || words_are_the_same(comp_type, COMP_TYPE_ATM_CHEM) || words_are_the_same(comp_type, COMP_TYPE_OCN) ||
		             words_are_the_same(comp_type, COMP_TYPE_LND) || words_are_the_same(comp_type, COMP_TYPE_SEA_ICE) || words_are_the_same(comp_type, COMP_TYPE_WAVE) || words_are_the_same(comp_type, COMP_TYPE_ROOT) || 
		             words_are_the_same(comp_type, COMP_TYPE_PSEUDO_COUPLED) || words_are_the_same(comp_type, COMP_TYPE_ACTIVE_COUPLED) || words_are_the_same(comp_type, COMP_TYPE_GLC) || words_are_the_same(comp_type, COMP_TYPE_RUNOFF), 
		             "cannot register component \"%s\" (the corresponding model code annotation is \"%s\") because its type \"%s\" is wrong", 
		             comp_name, annotation, comp_type);	
	if (parent != NULL && words_are_the_same(comp_type, COMP_TYPE_PSEUDO_COUPLED))
		EXECUTION_REPORT(REPORT_ERROR, -1, words_are_the_same(parent->comp_type, COMP_TYPE_PSEUDO_COUPLED) || words_are_the_same(parent->comp_type, COMP_TYPE_ROOT), 
		                 "Error happens when calling API \"CCPL_register_component\" to register a component model \"%s\" of type \"pesudo_coupled_system\": the type of its parent component model \"%s\" is \"%s\" but not \"pesudo_coupled_system\". Please check the model code related to the annotation \"%s\". Please verify", 
		                 comp_name, parent->comp_name, parent->comp_type, annotation);
/*
	if (parent != NULL && words_are_the_same(parent->comp_type, COMP_TYPE_PSEUDO_COUPLED) && parent->children.size() > 0)
		EXECUTION_REPORT(REPORT_ERROR, -1, false, 
		                 "Error happens when calling API \"CCPL_register_component\" to register a component model \"%s\" at the model code with the annotation \"%s\": its parent \"%s\" is a coupled system (type is \"pesudo_coupled_system\") that can only have one child in a process while it already has a child \"%s\" (registerd at the model code with the annotation \"%s\"). Please verify.", 
		                 comp_name, annotation, parent->comp_name, parent->children[0]->comp_name, parent->children[0]->get_annotation_start());
*/
	EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Comm_rank(comm_group, &current_proc_local_id) == MPI_SUCCESS);
	EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Comm_rank(MPI_COMM_WORLD, &current_proc_global_id) == MPI_SUCCESS);
	EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Comm_size(comm_group, &num_procs) == MPI_SUCCESS);
	processes_global_id = new int [num_procs];
	EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Allgather(&current_proc_global_id, 1, MPI_INT, processes_global_id, 1, MPI_INT, comm_group) == MPI_SUCCESS);
	for (i = 0; i < num_procs; i ++)
		local_processes_global_ids.push_back(processes_global_id[i]);
	delete [] processes_global_id;

	if (parent != NULL) {
		for (i = 0; i < local_processes_global_ids.size(); i ++) {
			for (j = 0; j < parent->local_processes_global_ids.size(); j ++)
				if (local_processes_global_ids[i] == parent->local_processes_global_ids[j])
					break;
			if (current_proc_local_id == 0)
				EXECUTION_REPORT(REPORT_ERROR,-1, j < parent->local_processes_global_ids.size(), 
				                 "The processes of component \"%s\" must be a subset of the processes of its parent component \"%s\". Please check the model code related to the annotations \"%s\" and \"%s\"", 
				                 comp_name, parent->get_comp_name(), annotation_start, parent->annotation_start);
		}
		parent->children.push_back(this);
	}
	
	if (ancestor != NULL) {
		if (is_real_component_model()) {
			sprintf(working_dir, "%s/CCPL_dir/run/data/%s", comp_comm_group_mgt_mgr->get_root_working_dir(), comp_type);
			create_directory(working_dir, comm_group, get_current_proc_local_id() == 0, false);
			sprintf(working_dir, "%s/CCPL_dir/run/data/%s/%s", comp_comm_group_mgt_mgr->get_root_working_dir(), comp_type, full_name);
			create_directory(working_dir, comm_group, get_current_proc_local_id() == 0, false);
		}
		sprintf(dir, "%s/CCPL_dir/run/CCPL_logs/by_components/%s", comp_comm_group_mgt_mgr->get_root_working_dir(), comp_type);
		create_directory(dir, comm_group, get_current_proc_local_id() == 0, false);	
		sprintf(dir, "%s/CCPL_dir/run/CCPL_logs/by_components/%s/%s", comp_comm_group_mgt_mgr->get_root_working_dir(), comp_type,full_name);
		create_directory(dir, comm_group, get_current_proc_local_id() == 0, false);		
		MPI_Barrier(get_comm_group());
		sprintf(comp_log_file_name, "%s/CCPL_dir/run/CCPL_logs/by_components/%s/%s/%s.CCPL.log.%d", comp_comm_group_mgt_mgr->get_root_working_dir(), comp_type, full_name, get_comp_name(), get_current_proc_local_id());
		sprintf(exe_log_file_name, "%s/CCPL_dir/run/CCPL_logs/by_executables/%s/%s.CCPL.log.%d", comp_comm_group_mgt_mgr->get_root_working_dir(), comp_comm_group_mgt_mgr->get_executable_name(), comp_comm_group_mgt_mgr->get_executable_name(), comp_comm_group_mgt_mgr->get_global_node_root()->get_current_proc_local_id());
	}

	if (parent != NULL)
		check_API_parameter_string(parent->get_comp_id(), API_ID_COMP_MGT_REG_COMP, comm_group, "registering a component model", comp_type, "\"comp_type\"", annotation);
	else check_API_parameter_string(-1, API_ID_COMP_MGT_REG_COMP, comm_group, "registering a component model", comp_type, "\"comp_type\"", annotation);

	if (current_proc_local_id == 0) {
		char XML_file_name[NAME_STR_SIZE];
		sprintf(XML_file_name, "%s/%s.basic_info.xml", comp_comm_group_mgt_mgr->get_components_processes_dir(), full_name);
		EXECUTION_REPORT(REPORT_ERROR, -1, !does_file_exist(XML_file_name), 
			             "Error happens when registering a component model \"%s\": another componet model with the same name has already been registered. Please check the model code related to the annotations \"%s\"", 
			             full_name, annotation);
		TiXmlDocument *XML_file = new TiXmlDocument;
		TiXmlDeclaration *XML_declaration = new TiXmlDeclaration(("1.0"),(""),(""));
		EXECUTION_REPORT(REPORT_ERROR, -1, XML_file != NULL, "Software error: cannot create an xml file");
		XML_file->LinkEndChild(XML_declaration);
		TiXmlElement *root_element = new TiXmlElement("Component");
		XML_file->LinkEndChild(root_element);
		write_node_into_XML(root_element);
		XML_file->SaveFile(XML_file_name);
		delete XML_file;
	}

	EXECUTION_REPORT_LOG(REPORT_LOG, -1, true, "Finish registering the component model \%s\"", full_name);
}


Comp_comm_group_mgt_node::Comp_comm_group_mgt_node(TiXmlElement *XML_element, const char *specified_full_name, const char *XML_file_name)
{
	int line_number;


	comp_id = -1;
	temp_array_buffer = NULL;
	proc_latest_model_time = NULL;
	EXECUTION_REPORT(REPORT_ERROR, -1, words_are_the_same(XML_element->Value(), "Online_Model"), "Software error in Comp_comm_group_mgt_node::Comp_comm_group_mgt_node: wrong element name");
	const char *XML_comp_name = get_XML_attribute(comp_id, 80, XML_element, "name", XML_file_name, line_number, "the name of the component model", "internal configuration file of component information");
	const char *XML_full_name = get_XML_attribute(comp_id, 512, XML_element, "full_name", XML_file_name, line_number, "the full_name of the component model", "internal configuration file of component information");
	EXECUTION_REPORT_LOG(REPORT_LOG, -1, true, "XML_full_name is %s\n", XML_full_name);
	const char *XML_processes = get_XML_attribute(comp_id, -1, XML_element, "processes", XML_file_name, line_number, "global IDs of the processes of the component model", "internal configuration file of component information");
	strcpy(this->comp_name, XML_comp_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, words_are_the_same(specified_full_name, XML_full_name), "Software error in Comp_comm_group_mgt_node::Comp_comm_group_mgt_node: the full name specified is different from the full name in XML file %s: %s vs %s", XML_file_name, specified_full_name, XML_full_name);
	strcpy(this->full_name, XML_full_name);
	int segment_start, segment_end;
	for (int i = 1; i < strlen(XML_processes)+1; i ++) {
		if (XML_processes[i-1] == ' ') {
			segment_start = XML_processes[i]-'0';
			segment_end = -1;
			EXECUTION_REPORT(REPORT_ERROR, -1, segment_start >= 0 && segment_start <= 9, "Software error in Comp_comm_group_mgt_node::Comp_comm_group_mgt_node: wrong format");
		}
		else if (XML_processes[i-1] == '~') {
			segment_end = XML_processes[i]-'0';
			EXECUTION_REPORT(REPORT_ERROR, -1, segment_end >= 0 && segment_end <= 9, "Software error in Comp_comm_group_mgt_node::Comp_comm_group_mgt_node: wrong format");
		}
		else if (XML_processes[i] == ' ' || XML_processes[i] == '\0') {
			if (segment_end == -1)
				local_processes_global_ids.push_back(segment_start);
			else {
				for (int j = segment_start; j <= segment_end; j ++)
					local_processes_global_ids.push_back(j);
			}
		}
		else if (XML_processes[i] != '~') {
			int digit = XML_processes[i] - '0';			
			EXECUTION_REPORT(REPORT_ERROR, -1, digit >= 0 && digit <= 9, "Software error in Comp_comm_group_mgt_node::Comp_comm_group_mgt_node: wrong format");
			if (segment_end == -1)
				segment_start = segment_start * 10 + digit;
			else segment_end = segment_end * 10 + digit;
		}
	}

	current_proc_local_id = -1;
	parent = NULL;
	restart_mgr = NULL;
}


void Comp_comm_group_mgt_node::transform_node_into_array()
{
	int num_procs, proc_id, num_children;


	num_procs = local_processes_global_ids.size();
	for (int i = 0; i < num_procs; i ++) {
		proc_id = local_processes_global_ids[i];
		write_data_into_array_buffer(&proc_id, sizeof(int), &temp_array_buffer, buffer_max_size, buffer_content_size);
	}
	write_data_into_array_buffer(&num_procs, sizeof(int), &temp_array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(&comm_group, sizeof(MPI_Comm), &temp_array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(comp_type, NAME_STR_SIZE, &temp_array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(comp_name, NAME_STR_SIZE, &temp_array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(full_name, NAME_STR_SIZE, &temp_array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(comp_log_file_name, NAME_STR_SIZE, &temp_array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(exe_log_file_name, NAME_STR_SIZE, &temp_array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(working_dir, NAME_STR_SIZE, &temp_array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(annotation_start, NAME_STR_SIZE, &temp_array_buffer, buffer_max_size, buffer_content_size);
	write_data_into_array_buffer(annotation_end, NAME_STR_SIZE, &temp_array_buffer, buffer_max_size, buffer_content_size);
}


void Comp_comm_group_mgt_node::reset_dir(Comp_comm_group_mgt_node *another_node)
{
	strcpy(working_dir, another_node->working_dir);
	strcpy(comp_log_file_name, another_node->comp_log_file_name);
	strcpy(exe_log_file_name, another_node->exe_log_file_name);
}


void Comp_comm_group_mgt_node::merge_comp_comm_info(const char *annotation)
{
	int i, num_children_at_root, *num_children_for_all, *counts, *displs;
	char *temp_buffer, status_file_name[NAME_STR_SIZE];
	int int_buffer_content_size;
	

	EXECUTION_REPORT(REPORT_ERROR,-1, !definition_finalized, 
		             "The registration related to component \"%s\" has been finalized before (the corresponding model code annotation is \"%s\"). It cannot be finalized again at the model code with the annotation \"%s\". Please verify.",
		             comp_name, annotation_end, annotation); 
	this->definition_finalized = true;

	sprintf(status_file_name, "%s/%s.end", comp_comm_group_mgt_mgr->get_comps_ending_config_status_dir(), comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id, "Original_grid_info")->get_full_name());
	if (current_proc_local_id == 0) {
		FILE *status_file = fopen(status_file_name, "w+");
		EXECUTION_REPORT(REPORT_ERROR, -1, status_file != NULL, "Software error in Comp_comm_group_mgt_node::merge_comp_comm_info: configuration ending status file cannot be created");
		fclose(status_file);
	}

	strcpy(this->annotation_end, annotation);

	for (int i = 0; i < children.size(); i ++)
		EXECUTION_REPORT(REPORT_ERROR,-1, children[i]->definition_finalized, 
		                 "The registration related to component \"%s\" has not been finalized yet. So the registration related to its parent component \"%s\" cannot be finalized. Please check the model code related to the annotations \"%s\" and \"%s\".",
		                 children[i]->comp_name, comp_name, children[i]->annotation_start, annotation_end);
	for (i = 0, num_children_at_root = 0; i < children.size(); i ++)
		if (children[i]->current_proc_local_id == 0) {
			num_children_at_root ++;
			write_data_into_array_buffer(children[i]->temp_array_buffer, children[i]->buffer_content_size, &temp_array_buffer, buffer_max_size, buffer_content_size);
		}
	
	if (current_proc_local_id == 0) {
		num_children_for_all = new int [local_processes_global_ids.size()];
		counts = new int [local_processes_global_ids.size()];
		displs = new int [local_processes_global_ids.size()];
	}

	EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Gather(&num_children_at_root, 1, MPI_INT, num_children_for_all, 1, MPI_INT, 0, comm_group) == MPI_SUCCESS);
	int_buffer_content_size = buffer_content_size;
	EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Gather(&int_buffer_content_size, 1, MPI_INT, counts, 1, MPI_INT, 0, comm_group) == MPI_SUCCESS);

	if (current_proc_local_id == 0) {
		displs[0] = 0;
		for (i = 1; i < local_processes_global_ids.size(); i ++) {
			displs[i] = displs[i-1]+counts[i-1];
			num_children_at_root += num_children_for_all[i];
		}
		temp_buffer = new char [displs[i-1]+counts[i-1]+100];
	}

    EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Gatherv(temp_array_buffer, buffer_content_size, MPI_CHAR, temp_buffer, counts, displs, MPI_CHAR, 0, comm_group) == MPI_SUCCESS);
	
	if (current_proc_local_id == 0) {		
		delete [] temp_array_buffer;
		temp_array_buffer = temp_buffer;
		buffer_content_size = displs[i-1]+counts[i-1];
		buffer_max_size = buffer_content_size+100;
		write_data_into_array_buffer(&num_children_at_root, sizeof(int), &temp_array_buffer, buffer_max_size, buffer_content_size);
		transform_node_into_array();
		delete [] num_children_for_all;
		delete [] counts;
		delete [] displs;
	}
	
	EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Bcast(&buffer_content_size, 1, MPI_LONG, 0, comm_group)  == MPI_SUCCESS);
	if (current_proc_local_id != 0) {
		delete [] temp_array_buffer;
		temp_array_buffer = new char [buffer_content_size];
		buffer_max_size = buffer_content_size;
	}
	EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Bcast(temp_array_buffer, buffer_content_size, MPI_CHAR, 0, comm_group)  == MPI_SUCCESS);
	buffer_content_iter = buffer_content_size;
}


void Comp_comm_group_mgt_node::write_node_into_XML(TiXmlElement *parent_element)
{
	int i, num_segments;
	int *segments_start, *segments_end;
	TiXmlElement * current_element;
	char *string;

	
	current_element = new TiXmlElement("Online_Model");
	parent_element->LinkEndChild(current_element);
	current_element->SetAttribute("name", comp_name);
	current_element->SetAttribute("full_name", full_name);
	current_element->SetAttribute("global_id", unified_global_id);

	segments_start = new int [local_processes_global_ids.size()];
	segments_end = new int [local_processes_global_ids.size()];
	segments_start[0] = local_processes_global_ids[0];
	for (i = 1, num_segments = 1; i < local_processes_global_ids.size(); i ++) {
		if (local_processes_global_ids[i] != local_processes_global_ids[i-1]+1) {
			segments_end[num_segments-1] = local_processes_global_ids[i-1];
			segments_start[num_segments] = local_processes_global_ids[i];
			num_segments ++;
		}
	}
	segments_end[num_segments-1] = local_processes_global_ids[local_processes_global_ids.size()-1];
	string = new char [num_segments*(8*2+1)];
	string[0] = '\0';
	for (i = 0; i < num_segments; i ++) {
		if (segments_start[i] != segments_end[i])
			sprintf(string+strlen(string), " %d~%d", segments_start[i], segments_end[i]);
		else sprintf(string+strlen(string), " %d", segments_start[i]);
	}

	current_element->SetAttribute("processes", string);
	
	delete [] segments_start;
	delete [] segments_end;
	delete [] string;

	for (i = 0; i < children.size(); i ++)
		children[i]->write_node_into_XML(current_element);
}


void Comp_comm_group_mgt_node::update_child(const Comp_comm_group_mgt_node *child_old, Comp_comm_group_mgt_node *child_new)
{
	int i;


	EXECUTION_REPORT(REPORT_ERROR, -1, words_are_the_same(child_old->full_name, child_new->full_name), "software error in Comp_comm_group_mgt_node::update_child: children names are not the same");
	for (i = 0; i < children.size(); i ++) {
		EXECUTION_REPORT(REPORT_ERROR, -1, children[i]->parent == this, "software error in Comp_comm_group_mgt_node::update_child: wrong parent1");
		EXECUTION_REPORT(REPORT_ERROR, -1, children[i]->parent != children[i], "software error in Comp_comm_group_mgt_node::update_child: wrong parent2");
		if (children[i] == child_old) {
			EXECUTION_REPORT_LOG(REPORT_LOG, child_old->comp_id, true, "Link the parent of component \"%s\" to \"%s\"", child_old->full_name, child_old->parent->get_full_name());
			child_new->parent = this;
			children[i] = child_new;
			break;
		}
	}

	EXECUTION_REPORT(REPORT_ERROR, -1, i < children.size(), "software error in Comp_comm_group_mgt_node::update_child");
}


void Comp_comm_group_mgt_node::transfer_data_buffer(Comp_comm_group_mgt_node *new_node)
{
	if (new_node->temp_array_buffer != NULL)
		delete [] new_node->temp_array_buffer;

	new_node->temp_array_buffer = this->temp_array_buffer;
	new_node->buffer_content_iter = this->buffer_content_iter;
	new_node->buffer_content_size = this->buffer_content_size;
	new_node->buffer_max_size = this->buffer_max_size;
	this->temp_array_buffer = NULL;
}


void Comp_comm_group_mgt_node::confirm_coupling_configuration_active(int API_id, bool require_real_model, const char *annotation)
{
	char API_label[NAME_STR_SIZE]; 

	get_API_hint(comp_id, API_id, API_label);
	EXECUTION_REPORT(REPORT_ERROR, comp_id, !definition_finalized, 
		             "ERROR happens when calling the API \"%s\" at the model code with the annotation \"%s\": the coupling configuration stage of the corresponding component model \"%s\" has been ended at the model code with the annotation \"%s\"", 
		             API_label, annotation, comp_name, get_annotation_end());
	if (require_real_model)
		EXECUTION_REPORT(REPORT_ERROR, comp_id, is_real_component_model(), 
			             "ERROR happens when calling the API \"%s\" at the model code with the annotation \"%s\": the corresponding component model \"%s\" cannot handle coupling configuration because it is a coupled system (its type is \"pesudo_coupled_system\"). Please verify.", 
			             API_label, annotation, comp_name);
}


int Comp_comm_group_mgt_node::get_local_proc_global_id(int local_indx)
{
    if (local_indx < local_processes_global_ids.size())
        return local_processes_global_ids[local_indx];

	EXECUTION_REPORT(REPORT_ERROR, -1, false, "Software error in Comp_comm_group_mgt_node::get_local_proc_global_id");
	
    return -1;
}


Comp_comm_group_mgt_node *Comp_comm_group_mgt_node::load_comp_info_from_XML(const char *comp_full_name)
{
	char XML_file_name[NAME_STR_SIZE];
	TiXmlDocument *XML_file;
	int i;


	sprintf(XML_file_name, "%s/%s.basic_info.xml", comp_comm_group_mgt_mgr->get_components_processes_dir(), comp_full_name);
	XML_file = open_XML_file_to_read(comp_id, XML_file_name, comm_group, true);
	TiXmlElement *XML_element = XML_file->FirstChildElement();
	TiXmlElement *Online_Model = XML_element->FirstChildElement();
	Comp_comm_group_mgt_node *pesudo_comp_node = new Comp_comm_group_mgt_node(Online_Model, comp_full_name, XML_file_name);
	delete XML_file;
		
	return pesudo_comp_node;
}


bool Comp_comm_group_mgt_node::is_real_component_model()
{ 
	return !words_are_the_same(comp_type, COMP_TYPE_PSEUDO_COUPLED) && !words_are_the_same(comp_type, COMP_TYPE_ROOT); 
}


bool Comp_comm_group_mgt_node::have_local_process(int local_proc_global_id)
{
	for (int i = 0; i < local_processes_global_ids.size(); i ++)
		if (local_proc_global_id == local_processes_global_ids[i])
			return true;
		
	return false;
}


void Comp_comm_group_mgt_node::allocate_proc_latest_model_time()
{
	if (proc_latest_model_time != NULL)
		return;
	
	EXECUTION_REPORT(REPORT_ERROR, -1, get_num_procs() > 0, "Software error in Comp_comm_group_mgt_node::allocate_proc_latest_model_time");
	proc_latest_model_time = new long [get_num_procs()];
	for (int i = 0; i < get_num_procs(); i ++)
		proc_latest_model_time[i] = -1;
}


void Comp_comm_group_mgt_node::set_current_proc_current_time(int days, int second)
{
	allocate_proc_latest_model_time();
	proc_latest_model_time[current_proc_local_id] = ((long)days)*((long)100000) + (long)second;
	EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "set_proc_latest_model_time %ld: %d %d", proc_latest_model_time[current_proc_local_id], days, second);
}


void Comp_comm_group_mgt_node::set_proc_latest_model_time(int proc_id, long model_time)
{
	allocate_proc_latest_model_time();
	EXECUTION_REPORT(REPORT_ERROR, -1, proc_id >= 0 && proc_id < get_num_procs(), "Software error in set_proc_latest_model_time: wrong proc id: %d vs %d", proc_id, get_num_procs());

	if (model_time > proc_latest_model_time[proc_id])
		proc_latest_model_time[proc_id] = model_time;
}


long Comp_comm_group_mgt_node::get_proc_latest_model_time(int proc_id)
{
	EXECUTION_REPORT(REPORT_ERROR, -1, proc_id >= 0 && proc_id < get_num_procs(), "Software error in get_proc_latest_model_time: wrong proc id");
	return proc_latest_model_time[proc_id];
}


void Comp_comm_group_mgt_node::get_all_descendant_real_comp_fullnames(int top_comp_id, std::vector<char*> &all_descendant_real_comp_fullnames, char **temp_array_buffer, long &buffer_max_size, long &buffer_content_size)
{
	char *local_temp_array_buffer = NULL, *gather_temp_array_buffer = NULL;
	long local_buffer_max_size, local_buffer_content_size = 0, gather_buffer_content_size = 0;
	

	for (int i = 0; i < children.size(); i ++)
		children[i]->get_all_descendant_real_comp_fullnames(top_comp_id, all_descendant_real_comp_fullnames, &local_temp_array_buffer, local_buffer_max_size, local_buffer_content_size);

	if (is_real_component_model() && current_proc_local_id == 0)
		dump_string(full_name, -1, &local_temp_array_buffer, local_buffer_max_size, local_buffer_content_size);

	if (current_proc_local_id != -1) {
		int *all_array_size = new int [get_num_procs()];
		gather_array_in_one_comp(get_num_procs(), current_proc_local_id, local_temp_array_buffer, local_buffer_content_size, sizeof(char), all_array_size, (void**)(&gather_temp_array_buffer), gather_buffer_content_size, comm_group);
		if (current_proc_local_id == 0)
			write_data_into_array_buffer(gather_temp_array_buffer, gather_buffer_content_size, temp_array_buffer, buffer_max_size, buffer_content_size);
		delete [] all_array_size;
	}

	if (local_temp_array_buffer != NULL)
		delete [] local_temp_array_buffer;
	if (gather_temp_array_buffer != NULL)
		delete [] gather_temp_array_buffer;

	if (comp_id == top_comp_id) {
		bcast_array_in_one_comp(current_proc_local_id, temp_array_buffer, buffer_content_size, comm_group);
		char temp_full_name[NAME_STR_SIZE];
		long str_size;
		while (buffer_content_size > 0) {
			load_string(temp_full_name, str_size, NAME_STR_SIZE, *temp_array_buffer, buffer_content_size, "C-Coupler internal");
			all_descendant_real_comp_fullnames.push_back(strdup(temp_full_name));
		}
		EXECUTION_REPORT(REPORT_ERROR, -1, buffer_content_size == 0, "Software error in Comp_comm_group_mgt_node::get_all_descendant_real_comp_fullnames");
		if (*temp_array_buffer != NULL) {
			delete [] *temp_array_buffer;
			*temp_array_buffer = NULL;
		}
	}
}


Comp_comm_group_mgt_mgr::Comp_comm_group_mgt_mgr(const char *executable_name)
{
	int i, j, num_procs, proc_id;
	char temp_string[NAME_STR_SIZE];
	std::vector<char*> unique_executable_name;

	
	global_node_array.clear();
	global_node_root = NULL;
	definition_finalized = false;
	EXECUTION_REPORT(REPORT_ERROR, -1, getcwd(root_working_dir,NAME_STR_SIZE) != NULL, 
		             "Cannot get the current working directory for running the model");

	for (i = strlen(executable_name)-1; i >= 0; i --)
		if (executable_name[i] == '/')
			break;
	i ++;
	EXECUTION_REPORT(REPORT_ERROR,-1, i < strlen(executable_name), "Software error1 in Comp_comm_group_mgt_mgr::Comp_comm_group_mgt_mgr");
	strcpy(this->executable_name, executable_name+i);	
	EXECUTION_REPORT(REPORT_ERROR, -1, !words_are_the_same(this->executable_name, "all"), "Error happens when using the executable \"%s\" to run the coupled system: the name of any executable cannot be all");

	EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Comm_rank(MPI_COMM_WORLD, &current_proc_global_id) == MPI_SUCCESS);
	EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Comm_size(MPI_COMM_WORLD, &num_total_global_procs) == MPI_SUCCESS);	

	sprintf(temp_string, "%s/CCPL_dir/run", root_working_dir);
	create_directory(temp_string, MPI_COMM_WORLD, current_proc_global_id == 0, false);
	sprintf(temp_string, "%s/CCPL_dir/run/CCPL_logs", root_working_dir);
	create_directory(temp_string, MPI_COMM_WORLD, current_proc_global_id == 0, true);
	sprintf(temp_string, "%s/CCPL_dir/run/CCPL_logs/by_components", root_working_dir);
	create_directory(temp_string, MPI_COMM_WORLD, current_proc_global_id == 0, false);
	sprintf(temp_string, "%s/CCPL_dir/run/data", root_working_dir);
	create_directory(temp_string, MPI_COMM_WORLD, current_proc_global_id == 0, false);
	sprintf(temp_string, "%s/CCPL_dir/run/data/all", root_working_dir);
	create_directory(temp_string, MPI_COMM_WORLD, current_proc_global_id == 0, true);
	sprintf(internal_H2D_grids_dir, "%s/CCPL_dir/run/data/all/internal_H2D_grids", root_working_dir);
	create_directory(internal_H2D_grids_dir, MPI_COMM_WORLD, current_proc_global_id == 0, true);
	sprintf(components_processes_dir, "%s/CCPL_dir/run/data/all/components_processes", root_working_dir);
	create_directory(components_processes_dir, MPI_COMM_WORLD, current_proc_global_id == 0, true);
	sprintf(comps_ending_config_status_dir, "%s/CCPL_dir/run/data/all/comps_ending_config_status", root_working_dir);
	create_directory(comps_ending_config_status_dir, MPI_COMM_WORLD, current_proc_global_id == 0, true);
	sprintf(runtime_config_root_dir, "%s/CCPL_dir/config", root_working_dir);
	first_active_comp_config_dir[0] = '\0';
	sprintf(coupling_flow_config_dir, "%s/CCPL_dir/run/data/all/coupling_flow_config", root_working_dir);
	create_directory(coupling_flow_config_dir, MPI_COMM_WORLD, current_proc_global_id == 0, true);
	sprintf(temp_string, "%s/CCPL_dir/run/CCPL_logs/by_executables", root_working_dir);
	create_directory(temp_string, MPI_COMM_WORLD, current_proc_global_id == 0, false);
	MPI_Barrier(MPI_COMM_WORLD);
	EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Comm_size(MPI_COMM_WORLD, &num_procs) == MPI_SUCCESS);
	EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Comm_rank(MPI_COMM_WORLD, &proc_id) == MPI_SUCCESS);
	char *all_executable_name, dir[NAME_STR_SIZE];
	if (proc_id == 0)
		all_executable_name = new char [NAME_STR_SIZE*num_procs];
	EXECUTION_REPORT(REPORT_ERROR,-1, MPI_Gather((char*)this->executable_name, NAME_STR_SIZE, MPI_CHAR, all_executable_name, NAME_STR_SIZE, MPI_CHAR, 0, MPI_COMM_WORLD) == MPI_SUCCESS);
	if (proc_id == 0) {
		unique_executable_name.push_back(all_executable_name);
		for (i = 1; i < num_procs; i ++) {
			for (j = 0; j < unique_executable_name.size(); j ++)
				if (words_are_the_same(unique_executable_name[j], all_executable_name+i*NAME_STR_SIZE)) {
					break;
				}
			if (j == unique_executable_name.size())
				unique_executable_name.push_back(all_executable_name+i*NAME_STR_SIZE);
		}
		for (int j = 0; j < unique_executable_name.size(); j ++) {
			sprintf(dir, "%s/CCPL_dir/run/CCPL_logs/by_executables/%s", root_working_dir, unique_executable_name[j]);
			create_directory(dir, MPI_COMM_WORLD, true, false);
		}
		delete [] all_executable_name;	
	}

	MPI_Barrier(MPI_COMM_WORLD);
}


Comp_comm_group_mgt_mgr::~Comp_comm_group_mgt_mgr()
{
	for (int i = 0; i < global_node_array.size(); i ++)
		delete global_node_array[i];

	delete [] sorted_comp_ids;
}


void Comp_comm_group_mgt_mgr::transform_global_node_tree_into_array(Comp_comm_group_mgt_node *current_global_node, Comp_comm_group_mgt_node **all_global_nodes, int &global_node_id)
{
	all_global_nodes[global_node_id++] = current_global_node;
	for (int i = 0; i < current_global_node->get_num_children(); i ++)
		transform_global_node_tree_into_array(current_global_node->get_child(i), all_global_nodes, global_node_id);
}


void Comp_comm_group_mgt_mgr::update_global_nodes(Comp_comm_group_mgt_node **all_global_nodes, int num_global_node)
{
	int i, j, old_global_array_size;


	for (i = 0; i < num_global_node; i ++)
		for (j = i+1; j < num_global_node; j ++)
			EXECUTION_REPORT(REPORT_ERROR, -1, !words_are_the_same(all_global_nodes[i]->get_comp_name(), all_global_nodes[j]->get_comp_name()), 
							 "There are at least two components have the same name (\"%s\"), which is not allowed. Please check the model code (%s and %s).", 
							 all_global_nodes[i]->get_comp_name(), all_global_nodes[i]->get_annotation_start(), all_global_nodes[j]->get_annotation_start());
		
	for (i = 0; i < global_node_array.size(); i ++) {
		for (j = 0; j < num_global_node; j ++)
			if (words_are_the_same(all_global_nodes[j]->get_comp_full_name(), global_node_array[i]->get_comp_full_name()))
				break;
		if (j == num_global_node)
			continue;
		all_global_nodes[j]->reset_comm_group(global_node_array[i]->get_comm_group());
		all_global_nodes[j]->reset_current_proc_local_id(global_node_array[i]->get_current_proc_local_id());
		all_global_nodes[j]->reset_local_node_id(global_node_array[i]->get_local_node_id());
		all_global_nodes[j]->reset_dir(global_node_array[i]);
		delete global_node_array[i];
		global_node_array[i] = all_global_nodes[j];
	}

	for (j = 0; j < num_global_node; j ++)
		if (all_global_nodes[j]->have_local_process(comp_comm_group_mgt_mgr->get_current_proc_global_id()))
			EXECUTION_REPORT(REPORT_ERROR, -1, all_global_nodes[j]->get_current_proc_local_id() != -1, "Software error in Comp_comm_group_mgt_mgr::update_global_nodes: fail1 to use component %s to replace", all_global_nodes[j]->get_full_name());
	
	old_global_array_size = global_node_array.size();
	for (i = 0; i < num_global_node; i ++) {
		for (j = 0; j < old_global_array_size; j ++)
			if (all_global_nodes[i] == global_node_array[j])
				break;
			if (j == old_global_array_size) {
				EXECUTION_REPORT(REPORT_ERROR, -1, all_global_nodes[i]->get_comp_id() == -1 && !all_global_nodes[i]->have_local_process(comp_comm_group_mgt_mgr->get_current_proc_global_id()), "Software error in Comp_comm_group_mgt_mgr::update_global_nodes: fail2 to use component %s to replace", all_global_nodes[i]->get_full_name());
				all_global_nodes[i]->reset_local_node_id(global_node_array.size()|TYPE_COMP_LOCAL_ID_PREFIX);
				global_node_array.push_back(all_global_nodes[i]);
			}
	}
}


bool Comp_comm_group_mgt_mgr::is_legal_local_comp_id(int local_comp_id)
{
	if ((local_comp_id&TYPE_ID_PREFIX_MASK) != TYPE_COMP_LOCAL_ID_PREFIX)
		return false;

	int true_parent_id = (local_comp_id & TYPE_ID_SUFFIX_MASK);
	return true_parent_id >= 0 && true_parent_id < global_node_array.size();
}


int Comp_comm_group_mgt_mgr::register_component(const char *comp_name, const char *comp_type, MPI_Comm &comm, int parent_local_id, int change_dir, const char *annotation)
{
	int i, true_parent_id = 0;
	Comp_comm_group_mgt_node *root_local_node, *new_comp;
	MPI_Comm global_comm = MPI_COMM_WORLD;
	
	
	if (definition_finalized)
		EXECUTION_REPORT(REPORT_ERROR, -1, !definition_finalized, 
		                 "Cannot register component \"%s\" at the model code with the annotation \"%s\" because the stage of registering coupling configurations of the whole coupled model has been ended at the model code with the annotation \"%s\"", 
		                 comp_name, annotation, global_node_array[0]->get_annotation_end());
	
	for (i = 0; i < global_node_array.size(); i ++)
		if (words_are_the_same(global_node_array[i]->get_comp_name(), comp_name))
			break;
		
	if (i < global_node_array.size())
		EXECUTION_REPORT(REPORT_ERROR, -1, i == global_node_array.size(),  
		                 "Error happens when registering a component model named \"%s\" at the model code with the annotation \"%s\": a component model with the same name has already been registered before at the model code with the annotation \"%s\".", 
		                 comp_name, annotation, global_node_array[i]->get_annotation_start());

	if (parent_local_id == -1) {
		root_local_node = new Comp_comm_group_mgt_node("ROOT", COMP_TYPE_ROOT, global_node_array.size()|TYPE_COMP_LOCAL_ID_PREFIX, NULL, global_comm, annotation);
		global_node_array.push_back(root_local_node);
		global_node_root = root_local_node;
		new_comp = new Comp_comm_group_mgt_node(comp_name, comp_type, global_node_array.size()|TYPE_COMP_LOCAL_ID_PREFIX, root_local_node, comm, annotation);
		global_node_array.push_back(new_comp);
	}
	else {
		true_parent_id = (parent_local_id & TYPE_ID_SUFFIX_MASK);
		EXECUTION_REPORT(REPORT_ERROR, -1, !global_node_array[true_parent_id]->is_definition_finalized(), 
			             "Cannot register component \"%s\" at the model code with the annotation \"%s\" because the registration corresponding to the parent \"%s\" has been ended at the model code with the annotation \"%s\"", 
			             comp_name, annotation, global_node_array[true_parent_id]->get_comp_name(), global_node_array[true_parent_id]->get_annotation_end()); // add debug information
		new_comp = new Comp_comm_group_mgt_node(comp_name, comp_type, global_node_array.size()|TYPE_COMP_LOCAL_ID_PREFIX, global_node_array[true_parent_id], comm, annotation);
		global_node_array.push_back(new_comp);
	}

	Comp_comm_group_mgt_node *temp_comp_node = new_comp->load_comp_info_from_XML(new_comp->get_full_name());
	delete temp_comp_node;

	EXECUTION_REPORT(REPORT_PROGRESS, new_comp->get_comp_id(), true, "The component model \"%s\" is successfully registered at the model code with the annotation \"%s\".", new_comp->get_full_name(), annotation);

	if (strlen(first_active_comp_config_dir) == 0 && new_comp->is_real_component_model()) {
		sprintf(first_active_comp_config_dir, "%s/%s", runtime_config_root_dir, new_comp->get_comp_name());
		if (change_dir == 1) {
			char new_dir[NAME_STR_SIZE];
			sprintf(new_dir, "%s/run/%s/%s/data", root_working_dir, new_comp->get_comp_type(), new_comp->get_comp_name());
			DIR *dir=opendir(new_dir);
			EXECUTION_REPORT(REPORT_ERROR, new_comp->get_comp_id(), dir != NULL, "Fail to change working directory for the first active component model \"%s\": the directory \"%s\" does not exist.", new_comp->get_comp_name(), new_dir);
			chdir(new_dir);
			EXECUTION_REPORT_LOG(REPORT_LOG, new_comp->get_comp_id(), true, "change working directory to\"%s\"", new_dir);
		}
		original_grid_mgr->initialize_CoR_grids();
	}

	return new_comp->get_local_node_id();
}


void Comp_comm_group_mgt_mgr::merge_comp_comm_info(int comp_local_id, const char *annotation)
{
	Comp_comm_group_mgt_node *global_node;
	int true_local_id = (comp_local_id & TYPE_ID_SUFFIX_MASK), global_node_id;


	global_node = global_node_array[true_local_id];
	global_node->merge_comp_comm_info(annotation);

	global_node_id = 0;
	Comp_comm_group_mgt_node *new_root = new Comp_comm_group_mgt_node(global_node, NULL, global_node_id);
	EXECUTION_REPORT(REPORT_ERROR, -1, global_node->get_buffer_content_iter() == 0, "Software error1 in Comp_comm_group_mgt_mgr::merge_comp_comm_info");
	Comp_comm_group_mgt_node **all_global_nodes = new Comp_comm_group_mgt_node *[global_node_id];
	global_node_id = 0;
	transform_global_node_tree_into_array(new_root, all_global_nodes, global_node_id);
	if (global_node->get_parent() != NULL)
		global_node->get_parent()->update_child(global_node, new_root);
	global_node->transfer_data_buffer(new_root);
	update_global_nodes(all_global_nodes, global_node_id);
	for (int i = 0; i < global_node_id; i ++)
		EXECUTION_REPORT(REPORT_ERROR, -1, all_global_nodes[i]->get_local_node_id() != -1, "Software error in Comp_comm_group_mgt_mgr::merge_comp_comm_info: wrong comp_id");

	delete [] all_global_nodes;

	global_node = global_node_array[true_local_id];
	MPI_Barrier(global_node->get_comm_group());

	if (true_local_id == 0) {
		EXECUTION_REPORT(REPORT_ERROR, -1, global_node_array.size() == global_node_id, "software error in Comp_comm_group_mgt_mgr::merge_comp_comm_info");
		global_node_root = global_node_array[0];
		definition_finalized = true;
		generate_sorted_comp_ids();
		write_comp_comm_info_into_XML();
//		read_comp_comm_info_from_XML();
	}

	if (true_local_id == 1)
		merge_comp_comm_info(global_node_array[0]->get_local_node_id(), annotation);

	check_validation();
}


void Comp_comm_group_mgt_mgr::generate_sorted_comp_ids()
{
	int i, j, k;
	
	sorted_comp_ids = new int [global_node_array.size()];
	sorted_comp_ids[0] = global_node_array.size();
	for (i = 1; i < global_node_array.size(); i ++)
		sorted_comp_ids[i] = -1;
	for (i = 1; i < global_node_array.size(); i ++) {
		for (k = 0, j = 1; j < global_node_array.size(); j ++) {
			if (i == j)
				continue;
			EXECUTION_REPORT(REPORT_ERROR, -1, !words_are_the_same(global_node_array[i]->get_full_name(), global_node_array[j]->get_full_name()), "in Comp_comm_group_mgt_mgr::generate_sorted_comp_ids");
			if (strcmp(global_node_array[i]->get_full_name(), global_node_array[j]->get_full_name()) > 0)
				k ++;
		}
		EXECUTION_REPORT(REPORT_ERROR, -1, sorted_comp_ids[k+1] == -1, "in Comp_comm_group_mgt_mgr::generate_sorted_comp_ids");		
		sorted_comp_ids[k+1] = global_node_array[i]->get_comp_id();
	}
	for (i = 1; i < global_node_array.size(); i ++)
		get_global_node_of_local_comp(sorted_comp_ids[i], "")->set_unified_global_id(i);
}


Comp_comm_group_mgt_node *Comp_comm_group_mgt_mgr::get_global_node_of_local_comp(int local_comp_id, const char *annotation)
{	
	EXECUTION_REPORT(REPORT_ERROR,-1, is_legal_local_comp_id(local_comp_id), 
		             "The id of component is wrong when getting the management node of a component. Please check the model code with the annotation \"%s\"", 
		             annotation); 

	return global_node_array[(local_comp_id&TYPE_ID_SUFFIX_MASK)];
}


MPI_Comm Comp_comm_group_mgt_mgr::get_comm_group_of_local_comp(int local_comp_id, const char *annotation)
{
	return get_global_node_of_local_comp(local_comp_id, annotation)->get_comm_group();
}


void Comp_comm_group_mgt_mgr::get_output_data_file_header(int comp_id, char *data_file_header)
{
	int true_comp_id;


	EXECUTION_REPORT(REPORT_ERROR, -1, is_legal_local_comp_id(comp_id), "software error in Comp_comm_group_mgt_mgr::get_data_file_header");
	true_comp_id = (comp_id & TYPE_ID_SUFFIX_MASK);	
	sprintf(data_file_header, "%s/%s", global_node_array[true_comp_id]->get_working_dir(), global_node_array[true_comp_id]->get_comp_name());
}


const char *Comp_comm_group_mgt_mgr::get_comp_log_file_name(int comp_id)
{
	int true_comp_id;


	EXECUTION_REPORT(REPORT_ERROR, -1, is_legal_local_comp_id(comp_id), "software error in Comp_comm_group_mgt_mgr::get_comp_log_file_name");
	true_comp_id = (comp_id & TYPE_ID_SUFFIX_MASK);
	return global_node_array[true_comp_id]->get_comp_log_file_name();
}


const char *Comp_comm_group_mgt_mgr::get_exe_log_file_name(int comp_id)
{
	int true_comp_id;


	if (comp_id == -1) {
		if (global_node_array.size() < 2)
			return NULL;
		return global_node_array[1]->get_exe_log_file_name();
	}

	true_comp_id = (comp_id & TYPE_ID_SUFFIX_MASK);
	return global_node_array[true_comp_id]->get_exe_log_file_name();
}


void Comp_comm_group_mgt_mgr::write_comp_comm_info_into_XML()
{
	char XML_file_name[NAME_STR_SIZE];

	
	TiXmlDocument *XML_file;
	TiXmlDeclaration *XML_declaration;
	TiXmlElement * root_element;


	if (current_proc_global_id != 0)
		return;

	XML_file = new TiXmlDocument;
	XML_declaration = new TiXmlDeclaration(("1.0"),(""),(""));
	EXECUTION_REPORT(REPORT_ERROR, -1, XML_file != NULL, "Software error: cannot create an xml file");
	
	XML_file->LinkEndChild(XML_declaration);
	root_element = new TiXmlElement("Components");
	XML_file->LinkEndChild(root_element);
	global_node_root->write_node_into_XML(root_element);

	sprintf(XML_file_name, "%s/components.xml", coupling_flow_config_dir);
	
	XML_file->SaveFile(XML_file_name);
	delete XML_file;
}


void Comp_comm_group_mgt_mgr::read_global_node_from_XML(const TiXmlElement *current_element)
{
	Comp_comm_group_mgt_node *global_node;
	const TiXmlNode *child;


	global_node = search_global_node(current_element->Attribute("full_name"));
	EXECUTION_REPORT(REPORT_ERROR, -1, global_node != NULL, "The XML file of component model hierarchy has been illegally modified by others or C-Coupler has software bugs");
	EXECUTION_REPORT(REPORT_ERROR, -1, words_are_the_same(global_node->get_comp_name(), current_element->Attribute("name")), "The XML file of component model hierarchy has been illegally modified by others or there are software errors");

	for (child = current_element->FirstChild(); child != NULL; child = child->NextSibling())
		read_global_node_from_XML(child->ToElement());
}


void Comp_comm_group_mgt_mgr::read_comp_comm_info_from_XML()
{
	char XML_file_name[NAME_STR_SIZE];


	sprintf(XML_file_name, "%s/components.xml", coupling_flow_config_dir);
	TiXmlDocument *XML_file = open_XML_file_to_read(-1, XML_file_name, MPI_COMM_NULL, true);
	
	TiXmlElement *XML_element = XML_file->FirstChildElement();
	TiXmlElement *Online_Model = XML_element->FirstChildElement();
	read_global_node_from_XML(Online_Model);
	delete XML_file;
}


Comp_comm_group_mgt_node *Comp_comm_group_mgt_mgr::search_comp_with_comp_name(const char *comp_name)
{
	for (int i = 0; i < global_node_array.size(); i ++)
		if (words_are_the_same(global_node_array[i]->get_comp_name(), comp_name) && global_node_array[i]->get_current_proc_local_id() != -1)
			return global_node_array[i];
		
	return NULL;	
}


void Comp_comm_group_mgt_mgr::check_validation()
{
	for (int i = 0; i < global_node_array.size(); i ++) {		
		EXECUTION_REPORT(REPORT_ERROR, -1, (global_node_array[i]->get_comp_id()&TYPE_ID_SUFFIX_MASK) == i, "Software error in Comp_comm_group_mgt_mgr::check_validation: wrong comp id does not match array index");
		EXECUTION_REPORT(REPORT_ERROR, -1, search_global_node(global_node_array[i]->get_comp_id()) == global_node_array[i], "Software error in Comp_comm_group_mgt_mgr::check_validation: wrong comp id");
		if (global_node_array[i]->get_parent() != NULL) {
			EXECUTION_REPORT(REPORT_ERROR, -1, (global_node_array[i]->get_comp_id()&TYPE_ID_SUFFIX_MASK) > (global_node_array[i]->get_parent()->get_comp_id()&TYPE_ID_SUFFIX_MASK), "Software error in Comp_comm_group_mgt_mgr::check_validation: wrong parent1: %s vs %s", global_node_array[i]->get_full_name(), global_node_array[i]->get_parent()->get_full_name());
			char full_name[NAME_STR_SIZE];
			if ( global_node_array[i]->get_parent()->is_real_component_model()) {
				sprintf(full_name, "%s@%s", global_node_array[i]->get_parent()->get_full_name(), global_node_array[i]->get_comp_name());
				EXECUTION_REPORT(REPORT_ERROR, -1, words_are_the_same(full_name, global_node_array[i]->get_full_name()), "Software error in Comp_comm_group_mgt_mgr::check_validation: wrong parent2: %s vs %s", global_node_array[i]->get_full_name(), global_node_array[i]->get_parent()->get_full_name());
			}
			EXECUTION_REPORT(REPORT_ERROR, -1, search_global_node(global_node_array[i]->get_parent()->get_full_name()) == global_node_array[i]->get_parent(), "Software error in Comp_comm_group_mgt_mgr::check_validation: wrong parent3: %s vs %s", global_node_array[i]->get_full_name(), global_node_array[i]->get_parent()->get_full_name());
		}	
	}	
}


Comp_comm_group_mgt_node *Comp_comm_group_mgt_mgr::search_global_node(const char *full_name)
{	
	for (int i = 0; i < global_node_array.size(); i ++)
		if (words_are_the_same(global_node_array[i]->get_full_name(), full_name)) {
			return global_node_array[i];
		}
		
	return NULL;	
}


Comp_comm_group_mgt_node *Comp_comm_group_mgt_mgr::search_global_node(int global_node_id)
{
	for (int i = 0; i < global_node_array.size(); i ++)
		if (global_node_array[i]->get_local_node_id() == global_node_id) {
			EXECUTION_REPORT(REPORT_ERROR, -1, get_global_node_of_local_comp(global_node_id,"") == global_node_array[i], "Software error in Comp_comm_group_mgt_mgr::search_global_node");
			return global_node_array[i];
		}
		
	return NULL;
}


int Comp_comm_group_mgt_mgr::get_current_proc_id_in_comp(int comp_id, const char *annotation)
{
	EXECUTION_REPORT(REPORT_ERROR, -1, is_legal_local_comp_id(comp_id), 
		             "The component id specified for getting the id of the current process is wrong. Please check the model code with the annotation %s.", 
		             annotation); 
	return search_global_node(comp_id)->get_current_proc_local_id();
}


int Comp_comm_group_mgt_mgr::get_num_proc_in_comp(int comp_id, const char *annotation)
{
	EXECUTION_REPORT(REPORT_ERROR, -1, is_legal_local_comp_id(comp_id), 
		             "The component id specified for getting the the number of processes is wrong. Please check the model code with the annotation %s.", 
		             annotation); 
	EXECUTION_REPORT(REPORT_ERROR, -1, search_global_node(comp_id)->get_current_proc_local_id() != -1, 
		             "The component id specified for getting the the number of processes is wrong. Please check the model code with the annotation %s.", 
		             annotation);
	return search_global_node(comp_id)->get_num_procs();
}


void Comp_comm_group_mgt_mgr::confirm_coupling_configuration_active(int comp_id, int API_id, bool require_real_model, const char *annotation)
{
	EXECUTION_REPORT(REPORT_ERROR, -1, comp_comm_group_mgt_mgr->is_legal_local_comp_id(comp_id), "software error in Comp_comm_group_mgt_mgr::confirm_coupling_configuration_active");
	get_global_node_of_local_comp(comp_id, "in Comp_comm_group_mgt_mgr::confirm_coupling_configuration_active")->confirm_coupling_configuration_active(API_id, require_real_model, annotation);
}


const int *Comp_comm_group_mgt_mgr::get_all_components_ids()
{
	int *all_components_ids = new int [global_node_array.size()];


	for (int i = 1; i < global_node_array.size(); i ++) {
		all_components_ids[i] = global_node_array[i]->get_comp_id();
	}
	all_components_ids[0] = global_node_array.size();

	return all_components_ids;
}


bool Comp_comm_group_mgt_mgr::has_comp_ended_configuration(const char *comp_full_name)
{
	char status_file_name[NAME_STR_SIZE];
	sprintf(status_file_name, "%s/%s.end", comps_ending_config_status_dir, comp_full_name);
	FILE *status_file = fopen(status_file_name, "r");
	if (status_file == NULL)
		return false;

	fclose(status_file);
	return true;
}


void Comp_comm_group_mgt_mgr::push_comp_node(Comp_comm_group_mgt_node *comp_node) 
{
	comp_node->reset_local_node_id(global_node_array.size()|TYPE_COMP_LOCAL_ID_PREFIX);
	global_node_array.push_back(comp_node); 
}


Comp_comm_group_mgt_node *Comp_comm_group_mgt_mgr::pop_comp_node()
{
	Comp_comm_group_mgt_node *top_comp_node = global_node_array[global_node_array.size()-1];
	global_node_array.erase(global_node_array.begin()+global_node_array.size()-1);
	return top_comp_node;
}


void Comp_comm_group_mgt_mgr::set_current_proc_current_time(int comp_id, int days, int second)
{
	get_global_node_of_local_comp(comp_id,"Comp_comm_group_mgt_mgr::set_current_proc_time")->set_current_proc_current_time(days, second);
}


Comp_comm_group_mgt_node *Comp_comm_group_mgt_mgr::load_comp_info_from_XML(int host_comp_id, const char *comp_full_name, MPI_Comm comm)
{
	char XML_file_name[NAME_STR_SIZE];
	TiXmlDocument *XML_file;
	int i;


	sprintf(XML_file_name, "%s/%s.basic_info.xml", comp_comm_group_mgt_mgr->get_components_processes_dir(), comp_full_name);
	XML_file = open_XML_file_to_read(host_comp_id, XML_file_name, comm, true);
	TiXmlElement *XML_element = XML_file->FirstChildElement();
	TiXmlElement *Online_Model = XML_element->FirstChildElement();
	Comp_comm_group_mgt_node *pesudo_comp_node = new Comp_comm_group_mgt_node(Online_Model, comp_full_name, XML_file_name);
	delete XML_file;
		
	return pesudo_comp_node;
}


