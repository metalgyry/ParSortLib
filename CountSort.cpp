/*
 * File: CountSort.cpp
 * Author: Egor Zhelamskiy
 * Date: June 20, 2025
 * Description: Sorting by counting for OpenCL.
 */
#include "ParSortLib.h"

// ������� ��������� ����� ����������� � �������� ���������
void __stdcall radix_func_cl_program(cl_program program, void* user_data)
{
	cl_device_id device = *(cl_device_id*)user_data;
	char str[5000]; cl_int err;
	err = clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 5000, (void*)str, NULL);
	if (err != CL_SUCCESS)
	{
		printf("\n\t������ ������ clGetProgramBuildInfo"); return;
	}
	if (str[0] == '\0' || str[0] == '\n') return;
	printf("\n\t��� OpenCL ��������:");
	printf("\n%s\n", str);
}

void countSort(cl_device_id* devices, cl_context* context, cl_command_queue* command_queue, cl_mem* arrays, cl_uint size_arr, cl_uint type_elem, cl_uint type_sort, cl_int min_num, cl_int max_num) {
	if (size_arr <= 2) {
		printf("\n(radixSort)Error: ��������� ������ ������� (size_arr <= 2)\n");
		return;
	}
	if (type_elem > 10) {
		printf("\n(radixSort)Error: This type_elem value was not found\n");
		return;
	}
	if (type_sort > 1) {
		printf("\n(radixSort)Error: This type_sort value was not found\n");
		return;
	}
	int szbff = max_num - min_num + 2; // +2 �.�. ������ ������� ����� ������ ����� 0, ��� ������ ���������
	int lnchnxtstp = 1;
	cl_int err, arg_int;
	cl_uint units;
	size_t SizeWork[3]{}, SizeGroup[3]{};
	// ��������� ���������� � ������������ ������� ������ ������� ���������
	err = clGetDeviceInfo(devices[0], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), (void*)&SizeGroup, NULL);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ ������ clGetDeviceInfo(CL_DEVICE_MAX_WORK_GROUP_SIZE)", err);
		return;
	}
	// ��������� ����� ���� ���������� �� ������ �� ������� ����������� ���� ������ ������� ���������
	err = clGetDeviceInfo(devices[0], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), (void*)&units, 0);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ ������ clGetDeviceInfo(CL_DEVICE_MAX_COMPUTE_UNITS)", err);
		return;
	}
	// ���������� ������������� ���������� ������� ���������, ��������� ��������� �� ������ ����������
	int SizeWorkMax = SizeGroup[0] * units;
	int szgroup = SizeGroup[0];
	// ��� ���������� ��� ���� ������������ ���������� 
	const char* opencl_source =
		// �������� ������� ����������:
		"__kernel void RadixSort (__global @* arr, __global int* bufarr, __global int* lnch2stp) {"
			"int idgl = get_global_id(0), szwr = get_global_size(0), #;"
			"if (*lnch2stp) {"
				"while (idgl < szarr) {"
					"atom_inc(&bufarr[arr[idgl] - lft + 1]);"
					"idgl += szwr;"
				"}"
			"} else {"
				"while (idgl < szbff) {"
					"$"
					"idgl += szwr;"
				"}"
			"}"
		"} \n";

	// ������������ ���������������� ���� ���� ������������ ����������
	int len = strlen(opencl_source);
	char* opencl_source_dinamic = new char[len + 200];
	int j = 0;
	for (int i = 0; i < len; i++)
	{
		if (!(strchr("@#$", opencl_source[i])))
		{
			opencl_source_dinamic[j] = opencl_source[i]; j++;
		}
		else {
			if (opencl_source[i] == '@')
			{
				switch (type_elem) {
				case 0: { strcpy_s(&opencl_source_dinamic[j], 5, "bool"); j += 4; break; }
				case 1: { strcpy_s(&opencl_source_dinamic[j], 5, "char"); j += 4; break; }
				case 2: { strcpy_s(&opencl_source_dinamic[j], 6, "uchar"); j += 5; break; }
				case 3: { strcpy_s(&opencl_source_dinamic[j], 6, "short"); j += 5; break; }
				case 4: { strcpy_s(&opencl_source_dinamic[j], 7, "ushort"); j += 6; break; }
				case 5: { strcpy_s(&opencl_source_dinamic[j], 4, "int"); j += 3; break; }
				case 6: { strcpy_s(&opencl_source_dinamic[j], 5, "uint"); j += 4; break; }
				case 7: { strcpy_s(&opencl_source_dinamic[j], 5, "long"); j += 4; break; }
				case 8: { strcpy_s(&opencl_source_dinamic[j], 6, "ulong"); j += 5; break; }
				default: break;
				}
			}
			if (opencl_source[i] == '#')
			{
				std::string str = "lft = " + std::to_string(min_num) + ", szbff = " + std::to_string(szbff) + ", szarr = " + std::to_string(size_arr) + ";";
				strcpy_s(&opencl_source_dinamic[j], str.size() + 1, str.c_str());
				j += str.length();
			}
			if (opencl_source[i] == '$')
			{
				std::string str = "for (int i = bufarr[idgl]; i < bufarr[idgl + 1]; i++) { arr[i] = ";
				if (type_sort) {
					str += "idgl + lft; }";
				}
				else {
					str += std::to_string(max_num) + " - idgl; }";
				}
				strcpy_s(&opencl_source_dinamic[j], str.size() + 1, str.c_str());
				j += str.length();
			}
		}
	}
	opencl_source_dinamic[j] = '\0';
	const char* opencl_source_const = opencl_source_dinamic;
	// ������� �������� � ��������� ������������ ������� �� context � ���� ����������
	cl_program opencl_program = clCreateProgramWithSource(*context, 1, &opencl_source_const, NULL, &arg_int);
	if (arg_int != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ �������� ������������ �������", arg_int);
		return;
	}
	// ������� ���������� � ������ ������������ ����� �� ������������ �������
	err = clBuildProgram(opencl_program, 1, devices, NULL, radix_func_cl_program, (void*)devices);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ ���������� ��� ������ ���������", err);
		return;
	}
	// ������� �������� � ����������� �������-����, ���������������� � ������� �������� ���� ������������ ����������
	cl_kernel kernel = clCreateKernel(opencl_program, "RadixSort", &arg_int);
	if (arg_int != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ �������� ������� ����", arg_int);
		return;
	}
	// ������� ���������� ���������(������������ ������ ������) ��� ���������� ����
	err = clSetKernelArg(kernel, 0, sizeof(arrays), (void*)arrays);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ ������ clSetKernelArg", err);
		return;
	}
	// ������� �������� ������� ������(�������� ������ ��� ������� ����������), ������ ������ � ���� � ���������� ��� ���������� ��� ���� 
	cl_mem mem_bufarr = clCreateBuffer(*context, CL_MEM_READ_WRITE, szbff * sizeof(int), NULL, &arg_int);
	if (arg_int != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ �������� ������� ������", arg_int);
		return;
	}
	int* bffarr = new int[szbff]{};
	err = clEnqueueWriteBuffer(*command_queue, mem_bufarr, CL_TRUE, 0, szbff * sizeof(int), (const void*)bffarr, 0, 0, 0);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ ������ clEnqueueWriteBuffer", err);
		return;
	}
	err = clSetKernelArg(kernel, 1, sizeof(mem_bufarr), (void*)&mem_bufarr);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ ������ clSetKernelArg", err);
		return;
	}
	// ������� �������� ������� ������(���� ���������� �������), ������ ������ � ���� � ���������� ��� ���������� ��� ���� 
	cl_mem mem_lnch2stp = clCreateBuffer(*context, CL_MEM_READ_WRITE, sizeof(int), NULL, &arg_int);
	if (arg_int != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ �������� ������� ������", arg_int);
		return;
	}
	err = clEnqueueWriteBuffer(*command_queue, mem_lnch2stp, CL_TRUE, 0, sizeof(int), (const void*)&lnchnxtstp, 0, 0, 0);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ ������ clEnqueueWriteBuffer", err);
		return;
	}
	err = clSetKernelArg(kernel, 2, sizeof(mem_lnch2stp), (void*)&mem_lnch2stp);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ ������ clSetKernelArg", err);
		return;
	}

	// ����������� ����� ����������� ������� ���������, ������� ������, ������ ������ � �������� ���� 
	SizeWork[0] = SizeWorkMax;
	if (size_arr < SizeWork[0]) {
		SizeWork[0] = size_arr;
		if (size_arr < SizeGroup[0]) {
			SizeGroup[0] = size_arr;
		}
	}
	err = clEnqueueNDRangeKernel(*command_queue, kernel, 1, NULL, SizeWork, SizeGroup, 0, 0, 0);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ ������� ������� ����", err);
		return;
	}
	err = clFinish(*command_queue);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ ������� ���������� ����", err);
		return;
	}
	// ��������� ������ �� ������� ������(�������� ������) � ���������� �������� ��� ������� �������
	err = clEnqueueReadBuffer(*command_queue, mem_bufarr, CL_TRUE, 0, szbff * sizeof(int), (void*)bffarr, 0, 0, 0);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ clEnqueueReadBuffer", err);
		return;
	}
	if (!type_sort) {
		for (int i = 1, k = (szbff - 1), bfr; i < ((szbff - 1) / 2); i++, k--) {
			bfr = bffarr[i];
			bffarr[i] = bffarr[k];
			bffarr[k] = bfr;
		}
	}
	for (int i = 2, indx = bffarr[1]; i < szbff; i++) {
		indx += bffarr[i];
		bffarr[i] = indx;
	}
	// ������ ������ � ������� ������(�������� ������ � ���� ���������� �������)
	err = clEnqueueWriteBuffer(*command_queue, mem_bufarr, CL_TRUE, 0, szbff * sizeof(int), (const void*)bffarr, 0, 0, 0);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ ������ clEnqueueWriteBuffer", err);
		return;
	}
	lnchnxtstp = 0;
	err = clEnqueueWriteBuffer(*command_queue, mem_lnch2stp, CL_TRUE, 0, sizeof(int), (const void*)&lnchnxtstp, 0, 0, 0);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ ������ clEnqueueWriteBuffer", err);
		return;
	}

	// ����������� ����� ����������� ������� ���������, ������� ������, ������ ������ � �������� ����
	SizeWork[0] = SizeWorkMax;
	SizeGroup[0] = szgroup;
	if (szbff < SizeWork[0]) {
		SizeWork[0] = szbff;
		if (szbff < SizeGroup[0]) {
			SizeGroup[0] = szbff;
		}
	}
	err = clEnqueueNDRangeKernel(*command_queue, kernel, 1, NULL, SizeWork, SizeGroup, 0, 0, 0);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ ������� ������� ����", err);
		return;
	}
	err = clFinish(*command_queue);
	if (err != CL_SUCCESS) {
		printf("\n(radixSort)Error %i: ������ ������� ���������� ����", err);
		return;
	}
	// ������������ OpenCL ��������
	clReleaseKernel(kernel);
	clReleaseProgram(opencl_program);
	clReleaseMemObject(mem_bufarr);
	clReleaseMemObject(mem_lnch2stp);

}

