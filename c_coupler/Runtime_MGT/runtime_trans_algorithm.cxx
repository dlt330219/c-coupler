/***************************************************************
 *  Copyright (c) 2017, Tsinghua University.
 *  This is a source file of C-Coupler.
 *  This file was initially finished by Dr. Cheng Zhang and then
 *  modified by Dr. Cheng Zhang and Dr. Li Liu. 
 *  If you have any problem, 
 *  please contact Dr. Cheng Zhang via zhangc-cess@tsinghua.edu.cn
 *  or Dr. Li Liu via liuli-cess@tsinghua.edu.cn
 ***************************************************************/



#include "runtime_trans_algorithm.h"
#include "global_data.h"
#include <string.h>
#include <unistd.h>


template <class T> void Runtime_trans_algorithm::pack_segment_data(T *mpi_buf, T *field_data_buf, int segment_start, int segment_size, int field_2D_size, int num_lev, bool is_V1D_sub_grid_after_H2D_sub_grid)
{
    int i, j, offset;

    if (is_V1D_sub_grid_after_H2D_sub_grid) {
        for (i = segment_start, offset = 0; i < segment_size+segment_start; i ++)
            for (j = 0; j < num_lev; j ++)
                mpi_buf[offset++] = field_data_buf[i+j*field_2D_size];
    }
    else {
        for (i = segment_start, offset = 0; i < segment_size+segment_start; i ++)
            for (j = 0; j < num_lev; j ++)
                mpi_buf[offset++] = field_data_buf[i*num_lev+j];        
    }
}


template <class T> void Runtime_trans_algorithm::unpack_segment_data(T *mpi_buf, T *field_data_buf, int segment_start, int segment_size, int field_2D_size, int num_lev, bool is_V1D_sub_grid_after_H2D_sub_grid)
{
    int i, j, offset;

    if (is_V1D_sub_grid_after_H2D_sub_grid) {
        for (i = segment_start, offset = 0; i < segment_size+segment_start; i ++)
            for (j = 0; j < num_lev; j ++)
                field_data_buf[i+j*field_2D_size] = mpi_buf[offset++];
    }
    else {
        for (i = segment_start, offset = 0; i < segment_size+segment_start; i ++)
            for (j = 0; j < num_lev; j ++)
                field_data_buf[i*num_lev+j] = mpi_buf[offset++];        
    }
}


Runtime_trans_algorithm::Runtime_trans_algorithm(bool send_or_receive, int num_transfered_fields, Field_mem_info ** fields_mem, Routing_info ** routers, MPI_Comm comm, int * ranks)
{
    bool only_have_no_decomp_data = true;


    this->send_or_receive = send_or_receive;
    this->num_transfered_fields = num_transfered_fields;
    EXECUTION_REPORT(REPORT_ERROR,-1, num_transfered_fields > 0, "Software error: Runtime_trans_algorithm does not have transfer fields");

    union_comm = comm;
    MPI_Comm_rank(union_comm, &current_proc_id_union_comm);

    this->fields_mem = new Field_mem_info *[num_transfered_fields];
    fields_data_buffers = new void *[num_transfered_fields];
    fields_routers = new Routing_info *[num_transfered_fields];
    last_history_receive_buffer_index = -1;
    last_field_remote_recv_count = -1;
    current_field_local_recv_count = 1;
	last_receive_sender_time = -1;

    for (int i = 0; i < num_transfered_fields; i ++) {
        this->fields_mem[i] = fields_mem[i];
        fields_data_buffers[i] = fields_mem[i]->get_data_buf();
        fields_routers[i] = routers[i];
    }

    if (send_or_receive) {
        local_comp_node = fields_routers[0]->get_src_comp_node();
        remote_comp_node = fields_routers[0]->get_dst_comp_node();
    }
    else {
        local_comp_node = fields_routers[0]->get_dst_comp_node();
        remote_comp_node = fields_routers[0]->get_src_comp_node();
    }
    strcpy(remote_comp_full_name, remote_comp_node->get_comp_full_name());
	remote_comp_node_updated = false;
    comp_id = local_comp_node->get_comp_id();
    current_proc_local_id = local_comp_node->get_current_proc_local_id();
    current_proc_global_id = comp_comm_group_mgt_mgr->get_current_proc_global_id();
    time_mgr = components_time_mgrs->get_time_mgr(comp_id);
	EXECUTION_REPORT(REPORT_ERROR, -1, time_mgr != NULL, "software error in Runtime_trans_algorithm::Runtime_trans_algorithm: wrong time mgr: %x: %d: %d: %s : %s: %s %s %s", comp_id, comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id, "C-Coupler native code get time manager")->get_current_proc_local_id(), current_proc_local_id, comp_comm_group_mgt_mgr->get_global_node_of_local_comp(comp_id, "C-Coupler native code get time manager")->get_comp_full_name(), local_comp_node->get_comp_full_name(), remote_comp_node->get_comp_full_name(), fields_routers[0]->get_src_comp_node()->get_comp_name(), fields_routers[0]->get_dst_comp_node()->get_comp_name());
    num_remote_procs = remote_comp_node->get_num_procs();
    num_local_procs = local_comp_node->get_num_procs();
    remote_proc_ranks_in_union_comm = new int [num_remote_procs];
    memcpy(remote_proc_ranks_in_union_comm, ranks, num_remote_procs*sizeof(int));
    sender_time_has_matched = false;

    transfer_size_with_remote_procs = new int [num_remote_procs];
    send_displs_in_remote_procs = new int [num_remote_procs];
    recv_displs_in_current_proc = new int [num_remote_procs];
    field_grids_num_lev = new long [num_transfered_fields];
    fields_data_type_sizes = new int [num_transfered_fields];
    is_V1D_sub_grid_after_H2D_sub_grid =  new bool [num_transfered_fields];

    memset(transfer_size_with_remote_procs, 0, sizeof(int)*num_remote_procs);
    memset(send_displs_in_remote_procs, 0, sizeof(int)*num_remote_procs);
    memset(recv_displs_in_current_proc, 0, sizeof(int)*num_remote_procs);

    only_have_no_decomp_data = true;
    for (int j = 0; j < num_remote_procs; j ++) {
        for (int i = 0; i < num_transfered_fields; i ++) {
            fields_data_type_sizes[i] = get_data_type_size(fields_mem[i]->get_data_type());
            is_V1D_sub_grid_after_H2D_sub_grid[i] = true;
            if (fields_routers[i]->get_num_dimensions() == 0) 
                field_grids_num_lev[i] = 1;
            else {
                field_grids_num_lev[i] = original_grid_mgr->get_num_grid_levels(fields_mem[i]->get_grid_id());
                only_have_no_decomp_data = false;
                transfer_size_with_remote_procs[j] += fields_routers[i]->get_num_elements_transferred_with_remote_proc(send_or_receive, j) * fields_data_type_sizes[i] * field_grids_num_lev[i];
                is_V1D_sub_grid_after_H2D_sub_grid[i] = original_grid_mgr->is_V1D_sub_grid_after_H2D_sub_grid(fields_mem[i]->get_grid_id());
            }    
        }
        if (transfer_size_with_remote_procs[j] > 0)
            index_remote_procs_with_common_data.push_back(j);
    }

    if (only_have_no_decomp_data) {
        if (send_or_receive) {
            num_remote_procs_related = num_remote_procs / num_local_procs;
            if (current_proc_local_id < (num_remote_procs % num_local_procs))
                num_remote_procs_related += 1;
            remote_proc_idx_begin = current_proc_local_id;
        }
        else {
            num_remote_procs_related = 1;
            remote_proc_idx_begin = current_proc_local_id % num_remote_procs;
        }

        for (int i = 0; i < num_remote_procs_related; i ++) {
            int remote_proc_idx = remote_proc_idx_begin + i * num_local_procs;
            for (int j = 0; j < num_transfered_fields; j ++)
                transfer_size_with_remote_procs[remote_proc_idx] += fields_data_type_sizes[j];
            index_remote_procs_with_common_data.push_back(remote_proc_idx);
        }
    }
    else {
        for (int j = 0; j < num_remote_procs; j ++)
            if (transfer_size_with_remote_procs[j] > 0)
                for (int i = 0; i < num_transfered_fields; i ++)
                    if (fields_routers[i]->get_num_dimensions() == 0)
                        transfer_size_with_remote_procs[j] += fields_data_type_sizes[i];
    }

    int * total_transfer_size_with_remote_procs = new int [num_local_procs * num_remote_procs];
    if (send_or_receive) {
        MPI_Allgather(transfer_size_with_remote_procs, num_remote_procs, MPI_INT, total_transfer_size_with_remote_procs, num_remote_procs, MPI_INT, local_comp_node->get_comm_group());
        for (int i = 0; i < current_proc_local_id; i ++) {
            for (int j = 0; j < num_remote_procs; j ++) {
                send_displs_in_remote_procs[j] += total_transfer_size_with_remote_procs[i*num_remote_procs+j] + 2*sizeof(long);
             }
        }
        for (int j = 0; j < num_remote_procs; j ++)
            send_displs_in_remote_procs[j] += sizeof(long)*2;
    }

    recv_displs_in_current_proc[0] = sizeof(long)*2;
    for (int i = 1; i < num_remote_procs; i ++)
        recv_displs_in_current_proc[i] = recv_displs_in_current_proc[i-1] + transfer_size_with_remote_procs[i-1] + 2*sizeof(long);

	current_receive_field_sender_time = -1;
	last_receive_field_sender_time = -1;
    data_buf_size = 0;
    for (int j = 0; j < num_remote_procs; j ++) 
        data_buf_size += transfer_size_with_remote_procs[j];

    total_buf_size = data_buf_size + (2*num_remote_procs + 2) * sizeof(long);
    total_buf = (char*) (new long[(total_buf_size+sizeof(long)-1)/sizeof(long)]);
    send_tag_buf = (long *) total_buf;
    
    send_tag_buf[0] = -1;
	send_tag_buf[1] = -1;
    for (int i = 0; i < num_remote_procs; i ++) {
        tag_buf = (long *) (total_buf + recv_displs_in_current_proc[i]);
        for (int j = 0; j < 2; j ++)
            tag_buf[j] = -1;
    }

    num_recv_procs_related = 0;
    recv_proc_start = -1;
    if (send_or_receive) {
        int color = (index_remote_procs_with_common_data.size() > 0) ? 1 : 0;
        MPI_Comm_split(local_comp_node->get_comm_group(), color, 0, &sub_comm);
        if (index_remote_procs_with_common_data.size() > 0) {
            for (int j = 0; j < num_remote_procs; j ++)
                for (int i = 0; i < num_local_procs; i ++)
                    if (total_transfer_size_with_remote_procs[i*num_remote_procs+j] > 0){
                        index_recv_procs_with_common_data.push_back(j);
                        break;
                    }

            int sub_rank, sub_size;
            MPI_Comm_rank(sub_comm, &sub_rank);
            MPI_Comm_size(sub_comm, &sub_size);
            num_recv_procs_related = index_recv_procs_with_common_data.size() / sub_size;
            if (sub_rank < index_recv_procs_with_common_data.size()%sub_size)
                num_recv_procs_related ++;
            recv_proc_start = sub_rank * num_recv_procs_related;
            if (sub_rank >= index_recv_procs_with_common_data.size()%sub_size)
                recv_proc_start += index_recv_procs_with_common_data.size() % sub_size;
        }
    }

    delete [] total_transfer_size_with_remote_procs;
}


Runtime_trans_algorithm::~Runtime_trans_algorithm()
{
    delete [] fields_mem;
    delete [] fields_data_buffers;
    delete [] fields_routers;
    delete [] field_grids_num_lev;
    delete [] fields_data_type_sizes;
    delete [] is_V1D_sub_grid_after_H2D_sub_grid;
	delete [] total_buf;
    delete [] transfer_size_with_remote_procs;
    delete [] send_displs_in_remote_procs;
    delete [] recv_displs_in_current_proc;
    delete [] remote_proc_ranks_in_union_comm;

    for (int i = 0; i < history_receive_sender_time.size(); i ++) {
        delete [] history_receive_data_buffer[i];
    }
}


void Runtime_trans_algorithm::pass_transfer_parameters(long current_remote_fields_time, int bypass_counter)
{
	this->current_remote_fields_time = current_remote_fields_time;
	this->bypass_counter = bypass_counter;
}


bool Runtime_trans_algorithm::set_local_tags()
{
    MPI_Win_lock(MPI_LOCK_SHARED, current_proc_id_union_comm, 0, data_win);
    send_tag_buf[0] = current_field_local_recv_count;
	send_tag_buf[1] = ((long)time_mgr->get_current_num_elapsed_day())*100000 + ((long)time_mgr->get_current_second());
    current_field_local_recv_count ++;
    MPI_Win_unlock(current_proc_id_union_comm, data_win);

    return true;
}


bool Runtime_trans_algorithm::is_remote_data_buf_ready(bool bypass_timer)
{
    long temp_field_remote_recv_count = -100;


    if (index_remote_procs_with_common_data.size() == 0)
        return true;

	if (comp_comm_group_mgt_mgr->get_is_definition_finalized() && !remote_comp_node_updated) {
		remote_comp_node = comp_comm_group_mgt_mgr->search_global_node(remote_comp_full_name);
		remote_comp_node_updated = true;
		remote_comp_node->allocate_proc_latest_model_time();
	}

    for (int i = 0; i < index_remote_procs_with_common_data.size(); i ++) {
        int remote_proc_index = index_remote_procs_with_common_data[i];
        if (transfer_size_with_remote_procs[remote_proc_index] > 0) {
 			if (remote_comp_node_updated && last_receive_sender_time < remote_comp_node->get_proc_latest_model_time(remote_proc_index)) {
				EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "Can bypass MPI_Get for proc %d", remote_proc_index);
				continue;
			}
            int remote_proc_id = remote_proc_ranks_in_union_comm[remote_proc_index];
            MPI_Win_lock(MPI_LOCK_SHARED, remote_proc_id, 0, data_win);
            MPI_Get(send_tag_buf, sizeof(long)*2, MPI_CHAR, remote_proc_id, 0, sizeof(long)*2, MPI_CHAR, data_win);
            MPI_Win_unlock(remote_proc_id, data_win);
			if (remote_comp_node_updated)
				remote_comp_node->set_proc_latest_model_time(remote_proc_index, send_tag_buf[1]);
			if (send_tag_buf[0] != -1 && send_tag_buf[0] != last_field_remote_recv_count + 1)
				return false;
			if (temp_field_remote_recv_count == -100)
				temp_field_remote_recv_count = send_tag_buf[0];
			if (temp_field_remote_recv_count != send_tag_buf[0])
				return false;
        }
    }

    if (temp_field_remote_recv_count == -1) {
        EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, last_field_remote_recv_count == -1 || last_field_remote_recv_count == 0, "Software error in Runtime_trans_algorithm::is_remote_data_buf_ready");
        if (last_field_remote_recv_count != -1) 
	        return false;
    }

    EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "Remote buffer component \"%s\" is ready for receiving data: %ld vs %ld vs %ld : %d", remote_comp_full_name, temp_field_remote_recv_count, last_field_remote_recv_count, last_receive_sender_time, bypass_counter);	

	last_field_remote_recv_count ++;
    return true;
}


void Runtime_trans_algorithm::receve_data_in_temp_buffer()
{
    bool is_ready = true;


    if (index_remote_procs_with_common_data.size() == 0)
        return;

    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, current_proc_id_union_comm, 0, data_win);
    for (int i = 0; i < index_remote_procs_with_common_data.size(); i ++) {
        int remote_proc_index = index_remote_procs_with_common_data[i];
        tag_buf = (long *) (total_buf + recv_displs_in_current_proc[remote_proc_index]);
        if (i == 0) {
            current_receive_field_sender_time = tag_buf[0];
            current_receive_field_usage_time = tag_buf[1];
        }
        else is_ready = is_ready && (current_receive_field_sender_time == tag_buf[0]);
    }
    MPI_Win_unlock(current_proc_id_union_comm, data_win);

    if (!is_ready)
        return;

    if (last_receive_field_sender_time == current_receive_field_sender_time)
        return;

    int empty_history_receive_buffer_index = -1;
    if (last_history_receive_buffer_index != -1) {
        for (int i = 0; i < history_receive_data_buffer.size(); i ++) {
            int index_iter = (last_history_receive_buffer_index+i) % history_receive_data_buffer.size();
            if (!history_receive_buffer_status[index_iter]) {
                empty_history_receive_buffer_index = index_iter;
                break;
            }
        }
    }
    if (empty_history_receive_buffer_index == -1) {
        std::vector<bool> temp_history_receive_buffer_status;
        std::vector<long> temp_history_receive_sender_time;
        std::vector<long> temp_history_receive_usage_time;
        std::vector<void*> temp_history_receive_data_buffer;
        for (int i = 0; i < history_receive_data_buffer.size(); i ++) {
            int index_iter = (last_history_receive_buffer_index+i) % history_receive_data_buffer.size();
            temp_history_receive_buffer_status.push_back(history_receive_buffer_status[index_iter]);
            temp_history_receive_sender_time.push_back(history_receive_sender_time[index_iter]);
            temp_history_receive_usage_time.push_back(history_receive_usage_time[index_iter]);
            temp_history_receive_data_buffer.push_back(history_receive_data_buffer[index_iter]);
        }
        history_receive_buffer_status.clear();
        history_receive_sender_time.clear();
        history_receive_usage_time.clear();
        history_receive_data_buffer.clear();
        for (int i = 0; i < temp_history_receive_data_buffer.size(); i ++) {
            history_receive_buffer_status.push_back(temp_history_receive_buffer_status[i]);
            history_receive_sender_time.push_back(temp_history_receive_sender_time[i]);
            history_receive_usage_time.push_back(temp_history_receive_usage_time[i]);
            history_receive_data_buffer.push_back(temp_history_receive_data_buffer[i]);            
        }
        last_history_receive_buffer_index = 0;
        empty_history_receive_buffer_index = history_receive_buffer_status.size();
        history_receive_buffer_status.push_back(false);
        history_receive_sender_time.push_back(-1);
        history_receive_usage_time.push_back(-1);
        history_receive_data_buffer.push_back(new long [(data_buf_size+sizeof(long)-1)/sizeof(long)]);
    }

    history_receive_buffer_status[empty_history_receive_buffer_index] = true;
    history_receive_sender_time[empty_history_receive_buffer_index] = current_receive_field_sender_time;
    history_receive_usage_time[empty_history_receive_buffer_index] = current_receive_field_usage_time;
    last_receive_field_sender_time = current_receive_field_sender_time;

    MPI_Win_lock(MPI_LOCK_SHARED, current_proc_id_union_comm, 0, data_win);
    //memcpy(history_receive_data_buffer[empty_history_receive_buffer_index], data_buf, data_buf_size);
    int offset = 0;
    for (int i = 0; i < index_remote_procs_with_common_data.size(); i ++) {
        int remote_proc_index = index_remote_procs_with_common_data[i];
        if (transfer_size_with_remote_procs[remote_proc_index] == 0) 
			continue;
        data_buf = (void *) (total_buf + recv_displs_in_current_proc[remote_proc_index] + 2*sizeof(long));
        memcpy((char *)history_receive_data_buffer[empty_history_receive_buffer_index]+offset, data_buf, transfer_size_with_remote_procs[remote_proc_index]);
        offset += transfer_size_with_remote_procs[remote_proc_index];
    }    
    MPI_Win_unlock(current_proc_id_union_comm, data_win);    

    EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "Get receiving data from component \"%s\" (at time %ld) into temp buffer", remote_comp_full_name, last_receive_field_sender_time);

    set_local_tags();
}


bool Runtime_trans_algorithm::run(bool bypass_timer)
{
    if (send_or_receive)
        return send(bypass_timer);
    else return recv(bypass_timer);
}


bool Runtime_trans_algorithm::send(bool bypass_timer)
{
    if (index_remote_procs_with_common_data.size() > 0) {
        preprocess();
        if (!is_remote_data_buf_ready(bypass_timer)) {
			inout_interface_mgr->runtime_receive_algorithms_receive_data();
            return false;
        }
    }

    for (int j = 0; j < num_transfered_fields; j ++) {
        fields_mem[j]->check_field_sum("before sending data");
        fields_mem[j]->use_field_values("before sending data");
    }  

    if (index_remote_procs_with_common_data.size() == 0)
        return true;

    long current_full_time = ((long)time_mgr->get_current_num_elapsed_day())*100000 + time_mgr->get_current_second();
    int offset = 0;
    //for (int i = 0; i < num_remote_procs; i ++) {
    for (int i = 0; i < index_remote_procs_with_common_data.size(); i ++) {
        int remote_proc_index = index_remote_procs_with_common_data[i];
        //if (transfer_size_with_remote_procs[remote_proc_index] == 0) continue;

        offset = 0;
        int old_offset = offset;
        data_buf = (void *) (total_buf + recv_displs_in_current_proc[remote_proc_index] + 2*sizeof(long));
        if (transfer_size_with_remote_procs[remote_proc_index] > 0)
            for (int j = 0; j < num_transfered_fields; j ++) {
                if (fields_routers[j]->get_num_dimensions() == 0) {
                    memcpy((char *)data_buf + offset, fields_data_buffers[j], fields_data_type_sizes[j]);
                    offset += fields_data_type_sizes[j];
                }
                else pack_MD_data(remote_proc_index, j, &offset);
            }

        tag_buf = (long *) (total_buf + recv_displs_in_current_proc[remote_proc_index]);
                if (bypass_timer) {
                    tag_buf[0] = current_full_time + bypass_counter*((long)100000000000000);
                    tag_buf[1] = -999;
                }
                else {
                    tag_buf[0] = current_full_time;
                    tag_buf[1] = current_remote_fields_time;
                }

        int remote_proc_id = remote_proc_ranks_in_union_comm[remote_proc_index];

        MPI_Win_lock(MPI_LOCK_SHARED, remote_proc_id, 0, data_win);
        MPI_Put(tag_buf, 2*sizeof(long)+transfer_size_with_remote_procs[remote_proc_index], MPI_CHAR, remote_proc_id, send_displs_in_remote_procs[remote_proc_index], 2*sizeof(long)+transfer_size_with_remote_procs[remote_proc_index], MPI_CHAR, data_win);
        MPI_Win_unlock(remote_proc_id, data_win);

        EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "Set remote tag to component \"%s\": %ld %ld", remote_comp_full_name, tag_buf[0], tag_buf[1]);
    }
    EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, offset <= data_buf_size, "Software error in Runtime_trans_algorithm::send: wrong data_buf_size: %d vs %d", offset, data_buf_size);

    if (bypass_timer)
        last_receive_sender_time = bypass_counter*((long)100000000000000);
    else last_receive_sender_time = current_remote_fields_time;

    EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "Finish sending data to component \"%s\"", remote_comp_full_name);

    return true;
}


bool Runtime_trans_algorithm::recv(bool bypass_timer)
{
    bool received_data_ready = false;
	

    if (bypass_timer) {
        EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "Bypass timer to begin to receive data from component \"%s\": %ld: %d", remote_comp_full_name, current_remote_fields_time, bypass_counter);
    }	
    else EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "Use timer to begin to receive data from component \"%s\": %ld", remote_comp_full_name, current_remote_fields_time);

    if (index_remote_procs_with_common_data.size() > 0) {

        preprocess();

        while (!received_data_ready) {
            receve_data_in_temp_buffer();
            received_data_ready = last_history_receive_buffer_index != -1 && history_receive_buffer_status[last_history_receive_buffer_index];
            if (!received_data_ready)
                inout_interface_mgr->runtime_receive_algorithms_receive_data();
        }

        int offset = 0;
        for (int i = 0; i < num_remote_procs; i ++) {
            if (transfer_size_with_remote_procs[i] == 0) 
                continue;
            int old_offset = offset;
            //int offset = recv_displs_in_current_proc[i];
            for (int j = 0; j < num_transfered_fields; j ++) {
                if (fields_routers[j]->get_num_dimensions() == 0) {
                    memcpy(fields_data_buffers[j], (char *) history_receive_data_buffer[last_history_receive_buffer_index] + offset, fields_data_type_sizes[j]);
                    offset += fields_data_type_sizes[j];
                }
                else unpack_MD_data(history_receive_data_buffer[last_history_receive_buffer_index], i, j, &offset);
                fields_mem[j]->define_field_values(false);
            }

            EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, offset - old_offset == transfer_size_with_remote_procs[i], "C-Coupler software error in recv of runtime_trans_algorithm.");
            //EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR, -1, offset - recv_displs_in_current_proc[i] == transfer_size_with_remote_procs[i], "C-Coupler software error in recv of runtime_trans_algorithm.");
        }
    }

	if (index_remote_procs_with_common_data.size() > 0)
		last_receive_sender_time = history_receive_sender_time[last_history_receive_buffer_index];
	else if (bypass_timer)
		last_receive_sender_time = bypass_counter*((long)100000000000000);
	else last_receive_sender_time = current_remote_fields_time;

    for (int j = 0; j < num_transfered_fields; j ++) {
         fields_mem[j]->check_field_sum("after receiving data");
         fields_mem[j]->define_field_values(false);
    }    

    if (index_remote_procs_with_common_data.size() > 0) {
        history_receive_buffer_status[last_history_receive_buffer_index] = false;
        last_history_receive_buffer_index = (last_history_receive_buffer_index+1) % history_receive_buffer_status.size();
    }

    EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "Finish receiving data from component \"%s\"", remote_comp_full_name);

    return true;
}


long Runtime_trans_algorithm::get_history_receive_sender_time(int j)
{
    return last_receive_sender_time;
}


void Runtime_trans_algorithm::preprocess()
{
    for (int i = 0; i < index_remote_procs_with_common_data.size(); i ++)
        transfer_size_with_remote_procs[index_remote_procs_with_common_data[i]] = 0;

    for (int i = 0; i < num_transfered_fields; i ++) {
        for (int j = 0; j < index_remote_procs_with_common_data.size(); j ++) {
            int remote_proc_index = index_remote_procs_with_common_data[j];
            if (fields_routers[i]->get_num_dimensions() == 0)
                transfer_size_with_remote_procs[remote_proc_index] += fields_data_type_sizes[i];
            else transfer_size_with_remote_procs[remote_proc_index] += fields_routers[i]->get_num_elements_transferred_with_remote_proc(send_or_receive, remote_proc_index) * fields_data_type_sizes[i] * field_grids_num_lev[i];
        }
    }
}


void Runtime_trans_algorithm::pack_MD_data(int remote_proc_index, int field_index, int * offset)
{
    int num_segments;
    int *segment_starts, *num_elements_in_segments;
    int i, j;
    int field_2D_size;


    num_segments = fields_routers[field_index]->get_num_local_indx_segments_with_remote_proc(true, remote_proc_index);
    if (num_segments == 0)
        return;

    segment_starts = fields_routers[field_index]->get_local_indx_segment_starts_with_remote_proc(true, remote_proc_index);
    num_elements_in_segments = fields_routers[field_index]->get_local_indx_segment_lengths_with_remote_proc(true, remote_proc_index);
    field_2D_size = fields_routers[field_index]->get_src_decomp_size();
    for (i = 0; i < num_segments; i ++) {
        switch (fields_data_type_sizes[field_index]) {
            case 1:
                pack_segment_data((char*)((char*)data_buf+(*offset)), (char*)fields_data_buffers[field_index], segment_starts[i], num_elements_in_segments[i], field_2D_size, field_grids_num_lev[field_index], is_V1D_sub_grid_after_H2D_sub_grid[field_index]);
                break;
            case 2:
                pack_segment_data((short*)((char*)data_buf+(*offset)), (short*)fields_data_buffers[field_index], segment_starts[i], num_elements_in_segments[i], field_2D_size, field_grids_num_lev[field_index], is_V1D_sub_grid_after_H2D_sub_grid[field_index]);
                break;
            case 4:
                pack_segment_data((int*)((char*)data_buf+(*offset)), (int*)fields_data_buffers[field_index], segment_starts[i], num_elements_in_segments[i], field_2D_size, field_grids_num_lev[field_index], is_V1D_sub_grid_after_H2D_sub_grid[field_index]);
                break;
            case 8:
                pack_segment_data((double*)((char*)data_buf+(*offset)), (double*)fields_data_buffers[field_index], segment_starts[i], num_elements_in_segments[i], field_2D_size, field_grids_num_lev[field_index], is_V1D_sub_grid_after_H2D_sub_grid[field_index]);
                break;
            default:
                EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR,-1, false, "Software error in Runtime_trans_algorithm::pack_MD_data: unsupported data type in runtime transfer algorithm. Please verify.");
                break;
        }
        (*offset) += num_elements_in_segments[i]*field_grids_num_lev[field_index]*fields_data_type_sizes[field_index];
    }
}


void Runtime_trans_algorithm::unpack_MD_data(void *data_buf, int remote_proc_index, int field_index, int * offset)
{
    int num_segments;
    int *segment_starts, *num_elements_in_segments;
    int i, j;
    int field_2D_size;


    num_segments = fields_routers[field_index]->get_num_local_indx_segments_with_remote_proc(false, remote_proc_index);
    if (num_segments == 0)
        return;

    segment_starts = fields_routers[field_index]->get_local_indx_segment_starts_with_remote_proc(false, remote_proc_index);
    num_elements_in_segments = fields_routers[field_index]->get_local_indx_segment_lengths_with_remote_proc(false, remote_proc_index);
    field_2D_size = fields_routers[field_index]->get_dst_decomp_size();
    for (i = 0; i < num_segments; i ++) {
        switch (fields_data_type_sizes[field_index]) {
            case 1:
                unpack_segment_data((char*)((char*)data_buf+(*offset)), (char*)fields_data_buffers[field_index], segment_starts[i], num_elements_in_segments[i], field_2D_size, field_grids_num_lev[field_index], is_V1D_sub_grid_after_H2D_sub_grid[field_index]);
                break;
            case 2:
                unpack_segment_data((short*)((char*)data_buf+(*offset)), (short*)fields_data_buffers[field_index], segment_starts[i], num_elements_in_segments[i], field_2D_size, field_grids_num_lev[field_index], is_V1D_sub_grid_after_H2D_sub_grid[field_index]);
                break;
            case 4:
                unpack_segment_data((int*)((char*)data_buf+(*offset)), (int*)fields_data_buffers[field_index], segment_starts[i], num_elements_in_segments[i], field_2D_size, field_grids_num_lev[field_index], is_V1D_sub_grid_after_H2D_sub_grid[field_index]);
                break;
            case 8:
                unpack_segment_data((double*)((char*)data_buf+(*offset)), (double*)fields_data_buffers[field_index], segment_starts[i], num_elements_in_segments[i], field_2D_size, field_grids_num_lev[field_index], is_V1D_sub_grid_after_H2D_sub_grid[field_index]);
                break;
            default:
                EXECUTION_REPORT_ERROR_OPTIONALLY(REPORT_ERROR,-1, false, "Software error in Runtime_trans_algorithm::unpack_MD_data: unsupported data type in runtime transfer algorithm. Please verify.");
                break;
        }
        (*offset) += num_elements_in_segments[i]*field_grids_num_lev[field_index]*fields_data_type_sizes[field_index];
    }
}

