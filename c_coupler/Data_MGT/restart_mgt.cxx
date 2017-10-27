/***************************************************************
  *  Copyright (c) 2017, Tsinghua University.
  *  This is a source file of C-Coupler.
  *  This file was initially finished by Dr. Li Liu. 
  *  If you have any problem, 
  *  please contact Dr. Li Liu via liuli-cess@tsinghua.edu.cn
  ***************************************************************/


#include "global_data.h"
#include "restart_mgt.h"


Restart_buffer_container::Restart_buffer_container(const char *comp_full_name, const char *buf_type, const char *keyword, const char *buffer_content, long buffer_content_iter)
{
	strcpy(this->comp_full_name, comp_full_name);
	strcpy(this->buf_type, buf_type);
	strcpy(this->keyword, keyword);
	this->buffer_content = buffer_content;
	this->buffer_content_iter = buffer_content_iter;
}


Restart_buffer_container::Restart_buffer_container(const char *array_buffer, long &buffer_content_iter, const char *file_name)
{
	long total_size, str_size;


	EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&total_size, sizeof(long), array_buffer, buffer_content_iter, file_name == NULL), "Fail to load the restart data file \"%s\": its format is wrong", file_name);
	
	buffer_content = load_string(NULL, str_size, -1, array_buffer, buffer_content_iter, file_name);
	this->buffer_content_iter = str_size;
	load_string(keyword, str_size, NAME_STR_SIZE, array_buffer, buffer_content_iter, file_name);
	load_string(buf_type, str_size, NAME_STR_SIZE, array_buffer, buffer_content_iter, file_name);
	load_string(comp_full_name, str_size, NAME_STR_SIZE, array_buffer, buffer_content_iter, file_name);
	EXECUTION_REPORT(REPORT_ERROR, -1, total_size == strlen(comp_full_name) + strlen(buf_type) + strlen(keyword) + sizeof(long)*3 + this->buffer_content_iter, "Restart_buffer_container::Restart_buffer_container: wrong format of restart data file");
}


void Restart_buffer_container::dump(char **array_buffer, long &buffer_max_size, long &buffer_content_size)
{
	dump_string(comp_full_name, -1, array_buffer, buffer_max_size, buffer_content_size);
	dump_string(buf_type, -1, array_buffer, buffer_max_size, buffer_content_size);
	dump_string(keyword, -1, array_buffer, buffer_max_size, buffer_content_size);
	dump_string(buffer_content, buffer_content_iter, array_buffer, buffer_max_size, buffer_content_size);
	long total_size = strlen(comp_full_name) + strlen(buf_type) + strlen(keyword) + sizeof(long)*3 + buffer_content_iter;
	write_data_into_array_buffer(&total_size, sizeof(long), array_buffer, buffer_max_size, buffer_content_size);
}


bool Restart_buffer_container::match(const char *buf_type, const char *keyword)
{
	return words_are_the_same(this->buf_type, buf_type) && words_are_the_same(this->keyword, keyword);
}


Restart_mgt::Restart_mgt(int comp_id)
{ 
	this->comp_id = comp_id; 
	last_timer_on_full_time = -1;
}


Restart_mgt::~Restart_mgt()
{
	clean();
}


void Restart_mgt::clean()
{
	for (int i = 0; i < restart_buffer_containers.size(); i ++)
		delete restart_buffer_containers[i];
	restart_buffer_containers.clear();
}


void Restart_mgt::bcast_buffer_container(Restart_buffer_container **buffer_container, int local_proc_id)
{
	char *temp_array_buffer = NULL;
	long buffer_max_size, buffer_content_size;


	if (local_proc_id == 0)
		(*buffer_container)->dump(&temp_array_buffer, buffer_max_size, buffer_content_size);
	
	transfer_array_from_one_comp_to_another(local_proc_id, -1, local_proc_id, -1, comp_comm_group_mgt_mgr->get_comm_group_of_local_comp(comp_id,""), &temp_array_buffer, buffer_content_size);
	
	if (local_proc_id != 0)
		*buffer_container = new Restart_buffer_container(temp_array_buffer, buffer_content_size, NULL);
	
	if (temp_array_buffer != NULL)
		delete [] temp_array_buffer;
}


Restart_buffer_container *Restart_mgt::search_restart_buffer(const char *buf_type, const char *keyword)
{
	for (int i = 0; i < restart_buffer_containers.size(); i ++)
		if (restart_buffer_containers[i]->match(buf_type, keyword))
			return restart_buffer_containers[i];

	return NULL;
}


Restart_buffer_container *Restart_mgt::search_then_bcast_buffer_container(const char *buf_type, const char *keyword, int local_proc_id)
{
	Restart_buffer_container *restart_buffer = NULL;

	if (local_proc_id == 0) {
		restart_buffer = search_restart_buffer(buf_type, keyword);
		if (restart_buffer == NULL)
			return NULL;
	}
	bcast_buffer_container(&restart_buffer, local_proc_id);
	if (local_proc_id != 0)
		restart_buffer_containers.push_back(restart_buffer);

	return restart_buffer;
}


void Restart_mgt::do_restart_read(const char *specified_file_name, const char *annotation)
{
	char restart_file_full_name[NAME_STR_SIZE], restart_file_short_name[NAME_STR_SIZE];
	

	time_mgr = components_time_mgrs->get_time_mgr(comp_id);
	
	if (words_are_the_same(time_mgr->get_run_type(), RUNTYPE_INITIAL)) {
		EXECUTION_REPORT(REPORT_PROGRESS, comp_id, true, "C-Coupler does not read the restart data file because it is a initial run (the run_type is initial)");
		return;
	}
	
	if (strlen(specified_file_name) == 0) {
		if (words_are_the_same(time_mgr->get_run_type(), RUNTYPE_CONTINUE))
			get_file_name_in_rpointer_file(restart_file_short_name);
		else sprintf(restart_file_short_name, "%s.%s.r.%08d-%05d", time_mgr->get_rest_refcase(), comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id,"")->get_comp_full_name(), time_mgr->get_rest_refdate(), time_mgr->get_rest_refsecond());
	}
	else strcpy(restart_file_short_name, specified_file_name);
	sprintf(restart_file_full_name, "%s/%s", comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id,"")->get_working_dir(), restart_file_short_name);
	do_restart_read(words_are_the_same(time_mgr->get_run_type(), RUNTYPE_CONTINUE) || words_are_the_same(time_mgr->get_run_type(), RUNTYPE_BRANCH), restart_file_full_name, annotation);
}


void Restart_mgt::do_restart_read(bool check_existing_data, const char *file_name, const char *annotation)
{
	int local_proc_id = comp_comm_group_mgt_mgr->get_current_proc_id_in_comp(comp_id, "in Restart_mgt::do_restart");


	time_mgr = components_time_mgrs->get_time_mgr(comp_id);

	if (local_proc_id == 0) {
		FILE *restart_fp = fopen(file_name, "r");
		EXECUTION_REPORT(REPORT_ERROR, comp_id, restart_fp != NULL, "Error happens when trying to read restart data at the model code with the annotation \"%s\": the data file \"%s\" does not exist. Please verify.", annotation, file_name);
		fseek(restart_fp, 0, SEEK_END);
		long buffer_content_iter = ftell(restart_fp);
		char *array_buffer = new char [buffer_content_iter];
		fseek(restart_fp, 0, SEEK_SET);
		fread(array_buffer, buffer_content_iter, 1, restart_fp);
		int num_restart_buffer_containers;
		EXECUTION_REPORT(REPORT_ERROR, -1, read_data_from_array_buffer(&num_restart_buffer_containers, sizeof(int), array_buffer, buffer_content_iter, false), "Fail to load the restart data file \"%s\": its format is wrong", file_name);
		for (int i = 0; i < num_restart_buffer_containers; i ++) {
			EXECUTION_REPORT(REPORT_ERROR, comp_id, buffer_content_iter > 0, "Software error in Restart_mgt::do_restart_read: wrong organization of restart data file");
			restart_buffer_containers.push_back(new Restart_buffer_container(array_buffer, buffer_content_iter, file_name));
		}
		EXECUTION_REPORT(REPORT_ERROR, comp_id, buffer_content_iter == 0, "Software error in Restart_mgt::do_restart_read: wrong organization of restart data file");
	}

	if (words_are_the_same(time_mgr->get_run_type(), RUNTYPE_CONTINUE) || words_are_the_same(time_mgr->get_run_type(), RUNTYPE_BRANCH)) {
		Restart_buffer_container *time_mgr_restart_buffer = search_then_bcast_buffer_container(RESTART_BUF_TYPE_TIME, "local time manager", local_proc_id);
		EXECUTION_REPORT(REPORT_ERROR, comp_id, time_mgr_restart_buffer != NULL, "Error happens when loading the restart data file \"%s\" at the model code with the annotation \"%s\": this file does not include the data for restarting the time information", file_name, annotation);
		long buffer_size = time_mgr_restart_buffer->get_buffer_content_iter();
		time_mgr->import_restart_data(time_mgr_restart_buffer->get_buffer_content(), buffer_size, file_name, check_existing_data);
		inout_interface_mgr->import_restart_data(this, file_name, local_proc_id, annotation);
	}

	clean();
}


void Restart_mgt::do_restart_write(const char *annotation, bool bypass_timer)
{
	char *array_buffer;
	long buffer_max_size, buffer_content_size;
	int local_proc_id = comp_comm_group_mgt_mgr->get_current_proc_id_in_comp(comp_id, "in Restart_mgt::do_restart");
	const char *comp_full_name = comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id,"in Restart_mgt::do_restart")->get_full_name();
	time_mgr = components_time_mgrs->get_time_mgr(comp_id);
	long current_full_time = time_mgr->get_current_full_time();


	if (bypass_timer || time_mgr->is_restart_timer_on()) {
		EXECUTION_REPORT(REPORT_ERROR, comp_id, current_full_time != last_timer_on_full_time, "Error happens when the component model tries to write restart data files: the corresponding API \"CCPL_do_restart_write\" has been called more than once in the same time step. Please verify the model code with the annotation \"%s\"");
		last_timer_on_full_time = current_full_time;
		if (local_proc_id == 0) {
			array_buffer = NULL;
			time_mgr->write_time_mgt_into_array(&array_buffer, buffer_max_size, buffer_content_size);
			restart_buffer_containers.push_back(new Restart_buffer_container(comp_full_name, RESTART_BUF_TYPE_TIME, "local time manager", array_buffer, buffer_content_size));
		}
		inout_interface_mgr->write_into_restart_buffers(&restart_buffer_containers, comp_id);
		if (local_proc_id == 0)
			write_into_file();
		clean();
	}
}


void Restart_mgt::get_file_name_in_rpointer_file(char *restart_file_name)
{
	char rpointer_file_name[NAME_STR_SIZE], line[NAME_STR_SIZE], *line_p;
	FILE *rpointer_file;


	sprintf(rpointer_file_name, "%s/rpointer", comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id,"")->get_working_dir());
	EXECUTION_REPORT(REPORT_ERROR, comp_id, does_file_exist(rpointer_file_name), "Error happens when try to restart data: file \"%s\" does not exist", rpointer_file_name);
	rpointer_file = fopen(rpointer_file_name, "r");
	get_next_line(line, rpointer_file);
	line_p = line;
	get_next_attr(restart_file_name, &line_p);
	fclose(rpointer_file);
}


void Restart_mgt::write_into_file()
{
	char *array_buffer = NULL;
	long buffer_max_size, buffer_content_size;
	int temp_int;
	char restart_file_name[NAME_STR_SIZE], rpointer_file_name[NAME_STR_SIZE];
	Comp_comm_group_mgt_node *comp_node = comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id,"in Restart_mgt::write_into_file");
	FILE *restart_file, *rpointer_file;
	

	for (int i = restart_buffer_containers.size()-1; i >= 0; i --)
		restart_buffer_containers[i]->dump(&array_buffer, buffer_max_size, buffer_content_size);
	temp_int = restart_buffer_containers.size();
	write_data_into_array_buffer(&temp_int, sizeof(int), &array_buffer, buffer_max_size, buffer_content_size);

	int date = last_timer_on_full_time/(long)100000;
	int second = last_timer_on_full_time%(long)100000;
	sprintf(restart_file_name, "%s/%s.%s.r.%08d-%05d", comp_node->get_working_dir(), time_mgr->get_case_name(), comp_node->get_comp_full_name(), date, second);
	restart_file = fopen(restart_file_name, "w+");
	EXECUTION_REPORT(REPORT_ERROR, comp_id, restart_file != NULL, "Failed to open the file \"%s\" for writing restart data", restart_file_name);
	fwrite(array_buffer, buffer_content_size, 1, restart_file);
	fclose(restart_file);
	delete [] array_buffer;	
	sprintf(rpointer_file_name, "%s/rpointer", comp_node->get_working_dir());
	rpointer_file = fopen(rpointer_file_name, "w+");	
	fprintf(rpointer_file, "%s.%s.r.%08d-%05d\n", time_mgr->get_case_name(), comp_node->get_comp_full_name(), date, second);
	fclose(rpointer_file);
}


