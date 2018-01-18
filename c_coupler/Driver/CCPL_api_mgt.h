/***************************************************************
  *  Copyright (c) 2017, Tsinghua University.
  *  This is a source file of C-Coupler.
  *  This file was initially finished by Dr. Li Liu. 
  *  If you have any problem, 
  *  please contact Dr. Li Liu via liuli-cess@tsinghua.edu.cn
  ***************************************************************/


#ifndef CCPL_API_MGT
#define CCPL_API_MGT


#include <mpi.h>
#include "tinyxml.h"


#define API_ID_FINALIZE                                 ((int)(0x00100001))
#define API_ID_COMP_MGT_REG_COMP                        ((int)(0x00200002))
#define API_ID_COMP_MGT_GET_COMP_LOG_FILE_NAME          ((int)(0x00200003))
#define API_ID_COMP_MGT_GET_COMP_LOG_FILE_DEVICE        ((int)(0x00200005))
#define API_ID_COMP_MGT_END_COMP_REG                    ((int)(0x00200004))
#define API_ID_COMP_MGT_IS_CURRENT_PROC_IN_COMP         ((int)(0x00200007))
#define API_ID_COMP_MGT_GET_CURRENT_PROC_ID_IN_COMP     ((int)(0x00200008))
#define API_ID_COMP_MGT_GET_NUM_PROC_IN_COMP            ((int)(0x00200010))
#define API_ID_COMP_MGT_GET_COMP_PROC_GLOBAL_ID         ((int)(0x00200011))
#define API_ID_COMP_MGT_GET_COMP_ID                     ((int)(0x00200020))
#define API_ID_COMP_MGT_IS_COMP_TYPE_COUPLED            ((int)(0x00200030))
#define API_ID_GRID_MGT_REG_H2D_GRID_VIA_LOCAL_DATA     ((int)(0X00400000))
#define API_ID_GRID_MGT_REG_H2D_GRID_VIA_GLOBAL_DATA    ((int)(0X00400001))
#define API_ID_GRID_MGT_REG_H2D_GRID_VIA_FILE           ((int)(0X00400002))
#define API_ID_GRID_MGT_REG_1D_GRID_ONLINE              ((int)(0X00400004))
#define API_ID_GRID_MGT_REG_MD_GRID_VIA_MULTI_GRIDS     ((int)(0X00400008))
#define API_ID_GRID_MGT_REG_GRID_VIA_COR                ((int)(0X00400010))
#define API_ID_GRID_MGT_REG_GRID_VIA_LOCAL              ((int)(0X00400020))
#define API_ID_GRID_MGT_REG_H2D_GRID_VIA_COMP           ((int)(0X00400040))
#define API_ID_GRID_MGT_CMP_GRID_VIA_REMOTE             ((int)(0X00400080))
#define API_ID_GRID_MGT_GET_GRID_ID                     ((int)(0X00400100))
#define API_ID_GRID_MGT_SET_GRID_DATA                   ((int)(0X00400200))
#define API_ID_GRID_MGT_SET_3D_GRID_DYN_BOT_FLD         ((int)(0X00400400))
#define API_ID_GRID_MGT_SET_3D_GRID_STATIC_BOT_FLD      ((int)(0X00400800))
#define API_ID_GRID_MGT_SET_3D_GRID_EXTERNAL_BOT_FLD    ((int)(0X00400a00))
#define API_ID_GRID_MGT_GET_H2D_GRID_DATA               ((int)(0X00402000))
#define API_ID_GRID_MGT_GET_H2D_GRID_AREA_FROM_WGTS     ((int)(0X00403000))
#define API_ID_GRID_MGT_REG_MID_POINT_GRID              ((int)(0X00404000))
#define API_ID_GRID_MGT_GET_GRID_SIZE                   ((int)(0X00408000))
#define API_ID_GRID_MGT_REG_V1D_Z_GRID_VIA_MODEL        ((int)(0X00410000))
#define API_ID_GRID_MGT_REG_V1D_SIGMA_GRID_VIA_MODEL    ((int)(0X00420000))
#define API_ID_GRID_MGT_REG_V1D_HYBRID_GRID_VIA_MODEL   ((int)(0X00440000))
#define API_ID_DECOMP_MGT_REG_DECOMP                    ((int)(0X00800001))
#define API_ID_FIELD_MGT_REG_FIELD_INST                 ((int)(0X01000001))
#define API_ID_FIELD_MGT_REG_IO_FIELD_from_INST         ((int)(0X01000002))
#define API_ID_FIELD_MGT_REG_IO_FIELDs_from_INSTs       ((int)(0X01000003))
#define API_ID_FIELD_MGT_REG_IO_FIELD_from_BUFFER       ((int)(0X01000004))
#define API_ID_TIME_MGT_SET_TIME_STEP                   ((int)(0X02000001))
#define API_ID_TIME_MGT_ADVANCE_TIME                    ((int)(0X02000002))
#define API_ID_TIME_MGT_RESET_TIME_TO_START             ((int)(0X02000003))
#define API_ID_TIME_MGT_DEFINE_SINGLE_TIMER             ((int)(0X02000004))
#define API_ID_TIME_MGT_DEFINE_COMPLEX_TIMER            ((int)(0X02000008))
#define API_ID_TIME_MGT_GET_CURRENT_NUM_DAYS_IN_YEAR	((int)(0X02100001))
#define API_ID_TIME_MGT_GET_CURRENT_YEAR                ((int)(0X02100002))
#define API_ID_TIME_MGT_GET_CURRENT_DATE                ((int)(0X02100004))
#define API_ID_TIME_MGT_GET_CURRENT_SECOND              ((int)(0X02100008))
#define API_ID_TIME_MGT_GET_START_TIME                  ((int)(0X02100010))	
#define API_ID_TIME_MGT_GET_STOP_TIME                   ((int)(0X02100020))
#define API_ID_TIME_MGT_GET_PREVIOUS_TIME               ((int)(0X02100040))
#define API_ID_TIME_MGT_GET_CURRENT_TIME                ((int)(0X02100080))
#define API_ID_TIME_MGT_GET_ELAPSED_DAYS_FROM_REF       ((int)(0X02100100))
#define API_ID_TIME_MGT_GET_ELAPSED_DAYS_FROM_START     ((int)(0X02100200))
#define API_ID_TIME_MGT_IS_END_CURRENT_DAY              ((int)(0X02100400))
#define API_ID_TIME_MGT_IS_END_CURRENT_MONTH            ((int)(0X02100800))
#define API_ID_TIME_MGT_GET_CURRENT_CAL_TIME            ((int)(0X02101000))
#define API_ID_TIME_MGT_IS_FIRST_STEP                   ((int)(0X02102000))
#define API_ID_TIME_MGT_IS_FIRST_RESTART_STEP           ((int)(0X02103000))
#define API_ID_TIME_MGT_GET_NUM_CURRENT_STEP            ((int)(0X02104000))
#define API_ID_TIME_MGT_GET_NUM_TOTAL_STEPS             ((int)(0X02108000))
#define API_ID_TIME_MGT_GET_TIME_STEP                   ((int)(0X02110000))
#define API_ID_TIME_MGT_CHECK_CURRENT_TIME              ((int)(0X02120000))
#define API_ID_TIME_MGT_IS_TIMER_ON                     ((int)(0X02140000))
#define API_ID_TIME_MGT_IS_MODEL_LAST_STEP              ((int)(0X02170000))
#define API_ID_TIME_MGT_IS_MODEL_RUN_ENDED              ((int)(0X02180000))
#define API_ID_INTERFACE_REG_IMPORT                     ((int)(0X04000001))
#define API_ID_INTERFACE_REG_EXPORT                     ((int)(0X04000002))
#define API_ID_INTERFACE_REG_NORMAL_REMAP               ((int)(0X04000003))
#define API_ID_INTERFACE_REG_FRAC_REMAP                 ((int)(0X04000004))
#define API_ID_INTERFACE_EXECUTE_WITH_ID                ((int)(0X04000005))
#define API_ID_INTERFACE_EXECUTE_WITH_NAME              ((int)(0X04000006))
#define API_ID_INTERFACE_CHECK_IMPORT_FIELD_CONNECTED   ((int)(0X04000007))
#define API_ID_INTERFACE_GET_LOCAL_COMP_FULL_NAME       ((int)(0X04000008))
#define API_ID_REPORT_LOG                               ((int)(0X06000001))
#define API_ID_REPORT_ERROR                             ((int)(0X06000002))
#define API_ID_REPORT_PROGRESS                          ((int)(0X06000004))
#define API_ID_RESTART_MGT_WRITE                        ((int)(0X07000001))
#define API_ID_RESTART_MGT_READ                         ((int)(0X07000002))
#define API_ID_RESTART_MGT_IS_TIMER_ON                  ((int)(0X07000004))
#define API_ID_COUPLING_GEN_FAMILY                      ((int)(0x08000001))
#define API_ID_COUPLING_GEN_EXTERNAL                    ((int)(0x08000002))
#define API_ID_COUPLING_GEN_INDIVIDUAL                  ((int)(0x08000003))
#define API_ID_COUPLING_GEN_GET_COMPS                   ((int)(0x08000004))


extern void synchronize_comp_processes_for_API(int, int, MPI_Comm, const char *, const char *);
extern void check_API_parameter_string(int, int, MPI_Comm, const char*, const char*, const char*, const char*);
extern void check_API_parameter_int(int, int, MPI_Comm, const char*, int, const char*, const char*);
extern void check_API_parameter_float(int, int, MPI_Comm, const char*, float, const char*, const char*);
extern void check_API_parameter_double(int, int, MPI_Comm, const char*, double, const char*, const char*);
extern void check_API_parameter_long(int, int, MPI_Comm, const char*, long, const char*, const char*);
extern void check_API_parameter_bool(int, int, MPI_Comm, const char *, bool, const char *, const char *);
extern void check_API_parameter_data_array(int, int, MPI_Comm, const char *, int, int, const char *, const char *, const char *);
extern void check_API_parameter_timer(int, int, MPI_Comm, const char*, int, const char*, const char*);
extern void check_API_parameter_field_instance(int, int, MPI_Comm, const char*, int, const char*, const char*);
extern void get_API_hint(int, int, char*);
extern void check_and_verify_name_format_of_string_for_API(int, const char*, int, const char*, const char*);
extern void check_and_verify_name_format_of_string_for_XML(int, const char*, const char*, const char*, int);
extern const char *get_XML_attribute(int, int, TiXmlElement*, const char*, const char*, int&, const char*, const char*);
extern bool is_XML_setting_on(int, TiXmlElement*, const char*, const char*, const char*);
extern void transfer_array_from_one_comp_to_another(int, int, int, int, MPI_Comm, char **, long &);
extern void gather_array_in_one_comp(int, int, void *, int, int, int *, void **, long &, MPI_Comm);
extern void bcast_array_in_one_comp(int, char **, long &, MPI_Comm);
extern long calculate_checksum_of_array(const char *, int, int, const char *, const char *);
extern char *check_and_aggregate_local_grid_data(int, int, MPI_Comm, const char *, int, int, int, char *, const char *, int, int *, int &, const char *);
extern bool does_file_exist(const char *);
extern TiXmlDocument *open_XML_file_to_read(int, const char *, MPI_Comm, bool);
extern TiXmlNode *get_XML_first_child_of_unique_root(int, const char *, TiXmlDocument *);


#endif

