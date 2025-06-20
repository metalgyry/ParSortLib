/*
 * File: ParSortLib.h
 * Author: Egor Zhelamskiy
 * Date: June 20, 2025
 * Description: Sorting library for OpenCL.
 */
#pragma once
#define CL_S_BOOL 0
#define CL_S_CHAR 1
#define CL_S_UCHAR 2
#define CL_S_SHORT 3
#define CL_S_USHORT 4
#define CL_S_INT 5
#define CL_S_UINT 6
#define CL_S_LONG 7
#define CL_S_ULONG 8
#define CL_S_FLOAT 9
#define CL_S_DOUBLE 10 

#define CL_S_ASC 1
#define CL_S_DESC 0

#define CL_S_AVRG_RAND 0
#define CL_S_AVRG_LFT 1
#define CL_S_AVRG_RGHT 2
#define CL_S_AVRG_MDDL 3

#include "CL/cl.h"
#include <iostream>
#include <string>

void timSort(cl_device_id* device, cl_context* context, cl_command_queue* command_queue, cl_mem* array, cl_uint size_arr, cl_uint type_elem, cl_uint type_sort);

void introSort(cl_device_id* device, cl_context* context, cl_command_queue* command_queue, cl_mem* array, cl_uint size_arr, cl_uint type_elem, cl_uint type_sort, cl_uint size_inst = 256, cl_uint avrg_elem = 1);

void countSort(cl_device_id* device, cl_context* context, cl_command_queue* command_queue, cl_mem* array, cl_uint size_arr, cl_uint type_elem, cl_uint type_sort, cl_int min_num, cl_int max_num);
