/***************************************************************
  *  Copyright (c) 2017, Tsinghua University.
  *  This is a source file of C-Coupler.
  *  This file was initially finished by Dr. Li Liu. 
  *  If you have any problem, 
  *  please contact Dr. Li Liu via liuli-cess@tsinghua.edu.cn
  ***************************************************************/


#include "global_data.h"
#include "runtime_cumulate_average_algorithm.h"
#include "runtime_config_dir.h"
#include "cor_global_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



template<typename T> void template_cumulate_or_average(T* dst, const T* src, const int length, 
        const int computing_count, const bool do_average)
{
    if (computing_count == 1) {
        for (int i = 0; i < length; i++)
            dst[i] = src[i];
    } 
    else {
        for (int i = 0; i < length; i++)
            dst[i] += src[i];
    }
    if (do_average && computing_count != 1) {
        /// a trick
        T frac = 1 / ((T)computing_count);
        if (frac == 0) {
            /// not a float number
            for (int i = 0; i < length; i++)
                dst[i] = dst[i] / computing_count;

        } else {
            /// float number
            for (int i = 0; i < length; i++)
                dst[i] = dst[i] * frac;
        }
    }
}


Runtime_cumulate_average_algorithm::Runtime_cumulate_average_algorithm(Field_mem_info *field_src, Field_mem_info *field_dst)
{
	cumulate_average_field_info *cumulate_average_field  = new cumulate_average_field_info;
	cumulate_average_field->mem_info_src = field_src;
	cumulate_average_field->mem_info_dst = field_dst;
	cumulate_average_field->timer = NULL;
	cumulate_average_field->num_elements_in_field = field_src->get_size_of_field();
	cumulate_average_field->field_data_type = field_src->get_data_type();
	cumulate_average_field->current_computing_count = 0;
    cumulate_average_fields.push_back(cumulate_average_field);

	comp_id = field_src->get_comp_id();
}


void Runtime_cumulate_average_algorithm::cumulate_or_average(bool do_average)
{
	EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "before cumulate or average");
	for (int i = 0; i < cumulate_average_fields.size(); i ++) {
		cumulate_average_fields[i]->mem_info_src->check_field_sum("(src value) before cumulate or average");
		cumulate_average_fields[i]->mem_info_dst->check_field_sum("(dst value) before cumulate or average");
	}
	
    for (int i = 0; i < cumulate_average_fields.size(); i ++) {
        cumulate_average_fields[i]->current_computing_count ++;
        if (words_are_the_same(cumulate_average_fields[i]->field_data_type, DATA_TYPE_FLOAT))
            template_cumulate_or_average<float>((float *) (cumulate_average_fields[i]->mem_info_dst->get_data_buf()), 
                                         (float *) (cumulate_average_fields[i]->mem_info_src->get_data_buf()), 
                                         cumulate_average_fields[i]->num_elements_in_field,
                                         cumulate_average_fields[i]->current_computing_count,
                                         do_average);
        else if (words_are_the_same(cumulate_average_fields[i]->field_data_type, DATA_TYPE_DOUBLE))
            template_cumulate_or_average<double>((double *) (cumulate_average_fields[i]->mem_info_dst->get_data_buf()), 
                                         (double *) (cumulate_average_fields[i]->mem_info_src->get_data_buf()), 
                                         cumulate_average_fields[i]->num_elements_in_field,
                                         cumulate_average_fields[i]->current_computing_count,
                                         do_average);
		else if (words_are_the_same(cumulate_average_fields[i]->field_data_type, DATA_TYPE_INT))
            template_cumulate_or_average<int>((int *) (cumulate_average_fields[i]->mem_info_dst->get_data_buf()), 
                                         (int *) (cumulate_average_fields[i]->mem_info_src->get_data_buf()), 
                                         cumulate_average_fields[i]->num_elements_in_field,
                                         cumulate_average_fields[i]->current_computing_count,
                                         do_average);
        else EXECUTION_REPORT(REPORT_ERROR, -1, false, "error data type in cumulate_average algorithm\n"); 
        if (do_average) {
			EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "do average at computing count is %d", cumulate_average_fields[i]->current_computing_count);
            cumulate_average_fields[i]->current_computing_count = 0;			
        }
		cumulate_average_fields[i]->mem_info_src->use_field_values(NULL);
		cumulate_average_fields[i]->mem_info_dst->define_field_values(false);
    }

	EXECUTION_REPORT_LOG(REPORT_LOG, comp_id, true, "after cumulate or average");
	for (int i = 0; i < cumulate_average_fields.size(); i ++) {
		cumulate_average_fields[i]->mem_info_dst->check_field_sum("(dst value) after cumulate or average");
	}
}


bool Runtime_cumulate_average_algorithm::run(bool is_algorithm_in_kernel_stage)
{
    cumulate_or_average(is_algorithm_in_kernel_stage);
	
	return true;
}


Runtime_cumulate_average_algorithm::~Runtime_cumulate_average_algorithm()
{
    for (int i = 0; i < cumulate_average_fields.size(); i ++) {
        delete cumulate_average_fields[i]->timer;
        delete cumulate_average_fields[i];
    }
}

