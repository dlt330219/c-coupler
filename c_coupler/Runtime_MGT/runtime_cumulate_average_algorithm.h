/***************************************************************
  *  Copyright (c) 2017, Tsinghua University.
  *  This is a source file of C-Coupler.
  *  This file was initially finished by Dr. Li Liu. 
  *  If you have any problem, 
  *  please contact Dr. Li Liu via liuli-cess@tsinghua.edu.cn
  ***************************************************************/


#ifndef RUNTIME_CUMULATE_AVERAGE_ALGORITHM
#define RUNTIME_CUMULATE_AVERAGE_ALGORITHM


#include "memory_mgt.h"
#include "common_utils.h"
#include <vector>


struct cumulate_average_field_info
{
    int num_elements_in_field;
    const char *field_data_type;
    Field_mem_info *mem_info_src;
    Field_mem_info *mem_info_dst;
    Coupling_timer *timer;
    int current_computing_count;
};


class Runtime_cumulate_average_algorithm
{
    private:
		int comp_id;
        std::vector<cumulate_average_field_info*> cumulate_average_fields;
        void cumulate_or_average(bool);
        
    public:
		Runtime_cumulate_average_algorithm(Field_mem_info*, Field_mem_info*);
		
        ~Runtime_cumulate_average_algorithm();
        bool run(bool);
};


#endif
