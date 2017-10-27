/***************************************************************
  *  Copyright (c) 2017, Tsinghua University.
  *  This is a source file of C-Coupler.
  *  This file was initially finished by Dr. Li Liu. 
  *  If you have any problem, 
  *  please contact Dr. Li Liu via liuli-cess@tsinghua.edu.cn
  ***************************************************************/


#include "runtime_remapping_weights_mgt.h"
#include "remap_operator_bilinear.h"
#include "remap_operator_linear.h"
#include "remap_operator_distwgt.h"
#include "remap_operator_conserv_2D.h"
#include "remap_operator_spline_1D.h"
#include "global_data.h"


Runtime_remapping_weights::Runtime_remapping_weights(int src_comp_id, int dst_comp_id, Original_grid_info *src_original_grid, Original_grid_info *dst_original_grid, Remapping_setting *remapping_setting, Decomp_info *dst_decomp_info)
{
	Remap_operator_basis *remap_operator_H2D = NULL;
	Remap_operator_basis *remap_operator_V1D = NULL;
	Remap_operator_basis *remap_operator_T1D = NULL;
	Remap_operator_basis *remap_operators[3];
	Remap_grid_class *remap_grids[2];
	Remapping_setting *cloned_remapping_setting = remapping_setting->clone();
	char parameter_name[NAME_STR_SIZE], parameter_value[NAME_STR_SIZE], remap_weight_name[NAME_STR_SIZE];
	int num_remap_operators = 0;
	H2D_remapping_wgt_file_info *H2D_remapping_weight_file = NULL;

	
	this->src_comp_id = src_comp_id;
	this->dst_comp_id = dst_comp_id;
	this->src_original_grid = src_original_grid;
	this->dst_original_grid = dst_original_grid;
	this->remapping_setting = cloned_remapping_setting;
	this->dst_decomp_info = dst_decomp_info;
	this->src_decomp_info = NULL;
	this->sequential_remapping_weights = NULL;
	this->parallel_remapping_weights = NULL;
	this->intermediate_V3D_grid_bottom_field = NULL;
	this->dynamic_V1D_remap_weight_of_operator = NULL;
	this->runtime_V1D_remap_grid_src = NULL;
	this->runtime_V1D_remap_grid_dst = NULL;

	if (src_original_grid->get_H2D_sub_CoR_grid() != NULL) {
		remap_grids[0] = src_original_grid->get_H2D_sub_CoR_grid();
		remap_grids[1] = dst_original_grid->get_H2D_sub_CoR_grid();
        if (words_are_the_same(cloned_remapping_setting->get_H2D_remapping_algorithm()->get_algorithm_name(), REMAP_OPERATOR_NAME_BILINEAR))
            remap_operator_H2D = new Remap_operator_bilinear("H2D_algorithm", 2, remap_grids);
        else if (words_are_the_same(cloned_remapping_setting->get_H2D_remapping_algorithm()->get_algorithm_name(), REMAP_OPERATOR_NAME_CONSERV_2D)) 
            remap_operator_H2D = new Remap_operator_conserv_2D("H2D_algorithm", 2,  remap_grids);
        else if (words_are_the_same(cloned_remapping_setting->get_H2D_remapping_algorithm()->get_algorithm_name(), REMAP_OPERATOR_NAME_DISTWGT))
            remap_operator_H2D = new Remap_operator_distwgt("H2D_algorithm", 2,  remap_grids);
        else EXECUTION_REPORT(REPORT_ERROR, -1, "Software error in Runtime_remapping_weights::Runtime_remapping_weights: wrong H2D algorithm");
		for (int i = 0; i < cloned_remapping_setting->get_H2D_remapping_algorithm()->get_num_parameters(); i ++) {
			cloned_remapping_setting->get_H2D_remapping_algorithm()->get_parameter(i, parameter_name, parameter_value);
			remap_operator_H2D->set_parameter(parameter_name, parameter_value);
		}
		remap_operators[num_remap_operators++] = remap_operator_H2D;
		H2D_remapping_weight_file = remapping_setting->search_H2D_remapping_weight(src_original_grid, dst_original_grid);
	}
	if (src_original_grid->get_V1D_sub_CoR_grid() != NULL) {
		remap_grids[0] = src_original_grid->get_V1D_sub_CoR_grid();
		remap_grids[1] = dst_original_grid->get_V1D_sub_CoR_grid();
        if (words_are_the_same(cloned_remapping_setting->get_V1D_remapping_algorithm()->get_algorithm_name(), REMAP_OPERATOR_NAME_LINEAR))
            remap_operator_V1D = new Remap_operator_linear("V1D_algorithm", 2, remap_grids);
        else if (words_are_the_same(cloned_remapping_setting->get_V1D_remapping_algorithm()->get_algorithm_name(), REMAP_OPERATOR_NAME_SPLINE_1D))
            remap_operator_V1D = new Remap_operator_spline_1D("V1D_algorithm", 2, remap_grids);
        else EXECUTION_REPORT(REPORT_ERROR, -1, "Software error in Runtime_remapping_weights::Runtime_remapping_weights: wrong V1D algorithm");
		for (int i = 0; i < cloned_remapping_setting->get_V1D_remapping_algorithm()->get_num_parameters(); i ++) {
			cloned_remapping_setting->get_V1D_remapping_algorithm()->get_parameter(i, parameter_name, parameter_value);
			remap_operator_V1D->set_parameter(parameter_name, parameter_value);
		}
		remap_operators[num_remap_operators++] = remap_operator_V1D;
	}
	if (src_original_grid->get_T1D_sub_CoR_grid() != NULL) {
		remap_grids[0] = src_original_grid->get_T1D_sub_CoR_grid();
		remap_grids[1] = dst_original_grid->get_T1D_sub_CoR_grid();
        if (words_are_the_same(cloned_remapping_setting->get_T1D_remapping_algorithm()->get_algorithm_name(), REMAP_OPERATOR_NAME_LINEAR))
            remap_operator_T1D = new Remap_operator_linear("T1D_algorithm", 2, remap_grids);
        else if (words_are_the_same(cloned_remapping_setting->get_T1D_remapping_algorithm()->get_algorithm_name(), REMAP_OPERATOR_NAME_SPLINE_1D))
            remap_operator_T1D = new Remap_operator_spline_1D("T1D_algorithm", 2, remap_grids);
        else EXECUTION_REPORT(REPORT_ERROR, -1, "Software error in Runtime_remapping_weights::Runtime_remapping_weights: wrong T1D algorithm");		
		for (int i = 0; i < cloned_remapping_setting->get_T1D_remapping_algorithm()->get_num_parameters(); i ++) {
			cloned_remapping_setting->get_T1D_remapping_algorithm()->get_parameter(i, parameter_name, parameter_value);
			remap_operator_T1D->set_parameter(parameter_name, parameter_value);
		}
		remap_operators[num_remap_operators++] = remap_operator_T1D;
	}

	execution_phase_number = 1;
	EXECUTION_REPORT(REPORT_ERROR, -1, num_remap_operators > 0, "Software error in Runtime_remapping_weights::Runtime_remapping_weights: no remapping operator");
	remapping_strategy = new Remap_strategy_class("runtime_remapping_strategy", num_remap_operators, remap_operators);
	EXECUTION_REPORT_LOG(REPORT_LOG, dst_decomp_info->get_host_comp_id(), true, "before generating sequential_remapping_weights from original grid %s to %s", src_original_grid->get_grid_name(), dst_original_grid->get_grid_name());	
	sprintf(remap_weight_name, "weights_%lx_%s(%s)_to_%s(%s)", remapping_setting->calculate_checksum(), src_original_grid->get_grid_name(), comp_comm_group_mgt_mgr->get_global_node_of_local_comp(src_comp_id,"")->get_full_name(), dst_original_grid->get_grid_name(), comp_comm_group_mgt_mgr->get_global_node_of_local_comp(dst_comp_id,"")->get_full_name());
	if (H2D_remapping_weight_file != NULL) {
		sequential_remapping_weights = new Remap_weight_of_strategy_class(remap_weight_name, remapping_strategy, src_original_grid->get_original_CoR_grid(), dst_original_grid->get_original_CoR_grid(), H2D_remapping_weight_file->get_wgt_file_name());
		H2D_remapping_weight_file->clean();
	}	
	else sequential_remapping_weights = new Remap_weight_of_strategy_class(remap_weight_name, remapping_strategy, src_original_grid->get_original_CoR_grid(), dst_original_grid->get_original_CoR_grid(), NULL);
	EXECUTION_REPORT_LOG(REPORT_LOG, dst_decomp_info->get_host_comp_id(), true, "after generating sequential_remapping_weights from original grid %s to %s", src_original_grid->get_grid_name(), dst_original_grid->get_grid_name());	
	execution_phase_number = 2;

	if (dst_original_grid->get_H2D_sub_CoR_grid() == NULL || dst_decomp_info == NULL) {
		EXECUTION_REPORT(REPORT_ERROR, -1, dst_original_grid->get_H2D_sub_CoR_grid() == NULL && dst_decomp_info == NULL, "Software error in Coupling_connection::generate_interpolation: conflict between grid and decomp");
		parallel_remapping_weights = sequential_remapping_weights;
	}	
	else {
		generate_parallel_remapping_weights();
		delete sequential_remapping_weights;
		sequential_remapping_weights = NULL;
	}
}


Runtime_remapping_weights::~Runtime_remapping_weights()
{
	delete remapping_setting;
	delete remapping_strategy;
	if (parallel_remapping_weights != NULL)
		delete parallel_remapping_weights;
	if (runtime_V1D_remap_grid_src != NULL)
		delete runtime_V1D_remap_grid_src;
	if (runtime_V1D_remap_grid_dst != NULL)
		delete runtime_V1D_remap_grid_dst;
}


Field_mem_info *Runtime_remapping_weights::allocate_intermediate_V3D_grid_bottom_field()
{
	if (intermediate_V3D_grid_bottom_field == NULL)
		intermediate_V3D_grid_bottom_field = memory_manager->alloc_mem("V3D_grid_bottom_field", dst_decomp_info->get_decomp_id(), decomps_info_mgr->get_decomp_info(dst_decomp_info->get_decomp_id())->get_grid_id(), -dst_original_grid->get_grid_id(), DATA_TYPE_DOUBLE, "unitless", "Runtime_remapping_weights::allocate_intermediate_V3D_grid_bottom_field", false);
	return intermediate_V3D_grid_bottom_field;
}


bool Runtime_remapping_weights::match_requirements(int src_comp_id, int dst_comp_id, Original_grid_info *src_original_grid, Original_grid_info *dst_original_grid, Remapping_setting *remapping_setting, Decomp_info *dst_decomp_info)
{
	return this->src_comp_id == src_comp_id && this->dst_comp_id == dst_comp_id && 
		   this->src_original_grid == src_original_grid && this->dst_original_grid == dst_original_grid && 
		   this->remapping_setting->is_the_same_as_another(remapping_setting) && this->dst_decomp_info == dst_decomp_info;
}


void Runtime_remapping_weights::generate_parallel_remapping_weights()
{
    Remap_grid_class **remap_related_grids, **remap_related_decomp_grids;
    Remap_grid_class *decomp_original_grids[256];
    int num_remap_related_grids;
    int *global_cells_local_indexes_in_decomps[256];
    int i, j;


    EXECUTION_REPORT(REPORT_ERROR,-1, sequential_remapping_weights != NULL, "C-Coupler software error remap weights is not found\n");
    cpl_check_remap_weights_format(sequential_remapping_weights);
	EXECUTION_REPORT(REPORT_ERROR,-1, src_original_grid->get_H2D_sub_CoR_grid()->is_subset_of_grid(sequential_remapping_weights->get_data_grid_src()) && dst_original_grid->get_H2D_sub_CoR_grid()->is_subset_of_grid(sequential_remapping_weights->get_data_grid_dst()),
	                 "Software error in Runtime_remapping_weights::generate_parallel_remapping_weights: grid inconsistency");

	EXECUTION_REPORT_LOG(REPORT_LOG, dst_decomp_info->get_comp_id(), true, "before generating remap_weights_src_decomp");
	src_decomp_info = decomps_info_mgr->generate_remap_weights_src_decomp(dst_decomp_info, src_original_grid, dst_original_grid, sequential_remapping_weights);
	EXECUTION_REPORT_LOG(REPORT_LOG, dst_decomp_info->get_comp_id(), true, "after generating remap_weights_src_decomp");
	EXECUTION_REPORT_LOG(REPORT_LOG, dst_decomp_info->get_comp_id(), true, "before generating parallel remap weights for runtime_remap_algorithm");
	if (src_decomp_info->get_num_local_cells() == 0)
		return;

    decomp_original_grids[0] = src_original_grid->get_H2D_sub_CoR_grid();
    decomp_original_grids[1] = dst_original_grid->get_H2D_sub_CoR_grid();
    remap_related_grids = sequential_remapping_weights->get_remap_related_grids(num_remap_related_grids);
    remap_related_decomp_grids = new Remap_grid_class *[num_remap_related_grids];

    for (i = 0; i < num_remap_related_grids; i ++) {
        j = 0;
		remap_related_decomp_grids[i] = remap_related_grids[i];
        if (decomp_original_grids[0]->is_subset_of_grid(remap_related_grids[i])) {
            remap_related_decomp_grids[i] = decomp_grids_mgr->search_decomp_grid_info(src_decomp_info->get_decomp_id(), remap_related_grids[i], false)->get_decomp_grid();
            j ++;
        }
        if (decomp_original_grids[1]->is_subset_of_grid(remap_related_grids[i])) {
			remap_related_decomp_grids[i] = decomp_grids_mgr->search_decomp_grid_info(dst_decomp_info->get_decomp_id(), remap_related_grids[i], false)->get_decomp_grid();
            j ++;
        }
		EXECUTION_REPORT(REPORT_ERROR, -1, j <= 1, "Software error in Runtime_remapping_weights::generate_parallel_remapping_weights: wrong j");
    }

    global_cells_local_indexes_in_decomps[0] = new int [decomp_original_grids[0]->get_grid_size()];
    global_cells_local_indexes_in_decomps[1] = new int [decomp_original_grids[1]->get_grid_size()];
	for (j = 0; j < decomp_original_grids[0]->get_grid_size(); j ++)
		global_cells_local_indexes_in_decomps[0][j] = -1;
	for (j = 0; j < src_decomp_info->get_num_local_cells(); j ++)
		if (src_decomp_info->get_local_cell_global_indx()[j] >= 0)
			global_cells_local_indexes_in_decomps[0][src_decomp_info->get_local_cell_global_indx()[j]] = j;
	for (j = 0; j < decomp_original_grids[1]->get_grid_size(); j ++)
		global_cells_local_indexes_in_decomps[1][j] = -1;
	for (j = 0; j < dst_decomp_info->get_num_local_cells(); j ++)
		if (dst_decomp_info->get_local_cell_global_indx()[j] >= 0)
			global_cells_local_indexes_in_decomps[1][dst_decomp_info->get_local_cell_global_indx()[j]] = j;  
	parallel_remapping_weights = sequential_remapping_weights->generate_parallel_remap_weights(remap_related_decomp_grids, decomp_original_grids, global_cells_local_indexes_in_decomps);
	dynamic_V1D_remap_weight_of_operator = parallel_remapping_weights->get_dynamic_V1D_remap_weight_of_operator();
	if (dynamic_V1D_remap_weight_of_operator != NULL) {
		runtime_V1D_remap_grid_src = dynamic_V1D_remap_weight_of_operator->get_field_data_grid_src()->generate_remap_operator_runtime_grid(dynamic_V1D_remap_weight_of_operator->get_original_remap_operator()->get_src_grid(),	dynamic_V1D_remap_weight_of_operator->get_original_remap_operator(), NULL);
		runtime_V1D_remap_grid_dst = dynamic_V1D_remap_weight_of_operator->get_field_data_grid_dst()->generate_remap_operator_runtime_grid(dynamic_V1D_remap_weight_of_operator->get_original_remap_operator()->get_dst_grid(),	dynamic_V1D_remap_weight_of_operator->get_original_remap_operator(), NULL);
	}

	if (dynamic_V1D_remap_weight_of_operator != NULL && get_dst_original_grid()->get_bottom_field_variation_type() != BOTTOM_FIELD_VARIATION_EXTERNAL && get_dst_original_grid()->get_bottom_field_variation_type() != BOTTOM_FIELD_VARIATION_UNSET) {
		if (dynamic_V1D_remap_weight_of_operator->get_field_data_grid_dst()->get_sigma_grid_dynamic_surface_value_field() != NULL)
			EXECUTION_REPORT(REPORT_ERROR, -1, dynamic_V1D_remap_weight_of_operator->get_field_data_grid_dst()->get_sigma_grid_dynamic_surface_value_field() == memory_manager->get_field_instance(get_dst_original_grid()->get_bottom_field_id())->get_field_data(), "Software error in Coupling_connection::add_bottom_field_coupling_info: the surface field of the same grid has been set to different data fields");
		else dynamic_V1D_remap_weight_of_operator->get_field_data_grid_dst()->set_sigma_grid_dynamic_surface_value_field(memory_manager->get_field_instance(get_dst_original_grid()->get_bottom_field_id())->get_field_data());
	}
	
	EXECUTION_REPORT_LOG(REPORT_LOG, dst_decomp_info->get_comp_id(), true, "after generating parallel remap weights for runtime_remap_algorithm");

	delete [] remap_related_decomp_grids;
	delete [] remap_related_grids;
	delete [] global_cells_local_indexes_in_decomps[0];
	delete [] global_cells_local_indexes_in_decomps[1];
}


void Runtime_remapping_weights::renew_dynamic_V1D_remapping_weights()
{
	bool src_bottom_value_updated = false, dst_bottom_value_updated = false;
	bool src_bottom_value_specified = false, dst_bottom_value_specified = false;

	
	if (dynamic_V1D_remap_weight_of_operator == NULL)
		return;

	if (dynamic_V1D_remap_weight_of_operator->get_field_data_grid_src()->is_sigma_grid()) {
		src_bottom_value_specified = dynamic_V1D_remap_weight_of_operator->get_field_data_grid_src()->is_sigma_grid_surface_value_field_specified();
		src_bottom_value_updated = dynamic_V1D_remap_weight_of_operator->get_field_data_grid_src()->is_sigma_grid_surface_value_field_updated();
		if (src_original_grid->get_bottom_field_variation_type() == BOTTOM_FIELD_VARIATION_STATIC)
			EXECUTION_REPORT(REPORT_ERROR, src_original_grid->get_comp_id(), !src_bottom_value_updated || !src_bottom_value_specified, "the surface field of the 3-D grid \"%s\" (registered in the component \"%s\") is updated while the surface field has been specified as a static one. Please verify", src_original_grid->get_grid_name(), comp_comm_group_mgt_mgr->get_global_node_of_local_comp(src_original_grid->get_comp_id(),"in Runtime_remapping_weights::renew_dynamic_V1D_remapping_weights")->get_full_name());
	}
	if (dynamic_V1D_remap_weight_of_operator->get_field_data_grid_dst()->is_sigma_grid()) {
		dst_bottom_value_specified = dynamic_V1D_remap_weight_of_operator->get_field_data_grid_dst()->is_sigma_grid_surface_value_field_specified();
		dst_bottom_value_updated = dynamic_V1D_remap_weight_of_operator->get_field_data_grid_dst()->is_sigma_grid_surface_value_field_updated();
		if (dst_original_grid->get_bottom_field_variation_type() == BOTTOM_FIELD_VARIATION_STATIC)
			EXECUTION_REPORT(REPORT_ERROR, dst_original_grid->get_comp_id(), !dst_bottom_value_updated || !dst_bottom_value_specified, "the surface field of the 3-D grid \"%s\" is updated while the surface field has been specified as a static one. Please verify", dst_original_grid->get_grid_name());
	}

	if (src_bottom_value_updated)
		dynamic_V1D_remap_weight_of_operator->get_field_data_grid_src()->calculate_lev_sigma_values();
	if (dst_bottom_value_updated)
		dynamic_V1D_remap_weight_of_operator->get_field_data_grid_dst()->calculate_lev_sigma_values();

	if (src_bottom_value_updated || dst_bottom_value_updated)
		dynamic_V1D_remap_weight_of_operator->renew_vertical_remap_weights(runtime_V1D_remap_grid_src, runtime_V1D_remap_grid_dst);
}


Runtime_remapping_weights_mgt::~Runtime_remapping_weights_mgt()
{
	for (int i = 0; i < runtime_remapping_weights.size(); i ++)
		delete runtime_remapping_weights[i];
}


Runtime_remapping_weights *Runtime_remapping_weights_mgt::search_or_generate_runtime_remapping_weights(int src_comp_id, int dst_comp_id, Original_grid_info *src_original_grid, Original_grid_info *dst_original_grid, Remapping_setting *remapping_setting, Decomp_info *dst_decomp_info)
{
	remapping_setting->shrink(src_original_grid, dst_original_grid);
	
	for (int i = 0; i < runtime_remapping_weights.size(); i ++)
		if (runtime_remapping_weights[i]->match_requirements(src_comp_id, dst_comp_id, src_original_grid, dst_original_grid, remapping_setting, dst_decomp_info))
			return runtime_remapping_weights[i];

	runtime_remapping_weights.push_back(new Runtime_remapping_weights(src_comp_id, dst_comp_id, src_original_grid, dst_original_grid, remapping_setting, dst_decomp_info));
	return runtime_remapping_weights[runtime_remapping_weights.size()-1];
}

