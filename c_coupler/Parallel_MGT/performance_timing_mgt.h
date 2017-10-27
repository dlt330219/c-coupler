/***************************************************************
  *  Copyright (c) 2017, Tsinghua University.
  *  This is a source file of C-Coupler.
  *  This file was initially finished by Dr. Li Liu. 
  *  If you have any problem, 
  *  please contact Dr. Li Liu via liuli-cess@tsinghua.edu.cn
  ***************************************************************/


#ifndef PERFORMANCE_TIMING_MGT_H
#define PERFORMANCE_TIMING_MGT_H


#include <vector>


#define TIMING_TYPE_COMMUNICATION         1
#define TIMING_TYPE_IO                    2
#define TIMING_TYPE_COMPUTATION           3

#define TIMING_COMMUNICATION_SEND        11
#define TIMING_COMMUNICATION_RECV        12
#define TIMING_COMMUNICATION_SENDRECV    13

#define TIMING_IO_INPUT                  21
#define TIMING_IO_OUTPUT                 22
#define TIMING_IO_RESTART                23


class Performance_timing_unit
{
	private:
		int unit_type;
		int unit_behavior;
		int unit_int_keyword;
		char unit_char_keyword[256];
		double previous_time;
		double total_time;

		void check_timing_unit(int, int, int, const char*);

	public:
		Performance_timing_unit(int, int, int, const char*);
		~Performance_timing_unit(){}
		void timing_start();
		void timing_stop();
		void timing_output();
		bool match_timing_unit(int, int, int, const char*);
};


class Performance_timing_mgt
{
	private:
		std::vector<Performance_timing_unit*> performance_timing_units;
		int search_timing_unit(int, int, int, const char*);

	public: 
		Performance_timing_mgt() {}
		~Performance_timing_mgt();
		void performance_timing_start(int, int, int, const char*);
		void performance_timing_stop(int, int, int, const char*);
		void performance_timing_output();
};

#endif
