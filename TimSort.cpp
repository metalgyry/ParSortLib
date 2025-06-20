/*
 * File: TimSort.cpp
 * Author: Egor Zhelamskiy
 * Date: June 20, 2025
 * Description: Timsort for OpenCL.
 */
#include "ParSortLib.h"

// Функция получения логов компилятора и сборщика программы
void __stdcall func_cl_program(cl_program program, void* user_data)
{
	cl_device_id device = *(cl_device_id*)user_data;
	char str[5000]; cl_int err;
	err = clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 5000, (void*)str, NULL);
	if (err != CL_SUCCESS)
	{
		printf("\n\tОшибка вызова clGetProgramBuildInfo"); return;
	}
	if (str[0] == '\0' || str[0] == '\n') return;
	printf("\n\tЛОГ OpenCL СБОРЩИКА:");
	printf("\n%s\n", str);
}

void timSort(cl_device_id* devices, cl_context* context, cl_command_queue* command_queue, cl_mem* arrays, cl_uint size_arr, cl_uint type_elem, cl_uint type_sort) {
	if (size_arr <= 2) {
		printf("\n(timSort)Error: Маленький размер массива (size_arr <= 2)\n");
		return;
	}
	if (type_sort > 1) {
		printf("\n(timSort)Error: This type_sort value was not found\n");
		return;
	}
	cl_int err, arg_int;
	cl_uint units;
	size_t SizeWork[3]{}, SizeGroup[3]{};

	// Получение информации о максимальном размере группы рабочих элементов
	err = clGetDeviceInfo(devices[0], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), (void*)&SizeGroup, NULL);
	if (err != CL_SUCCESS) {
		printf("\n(timSort)Error %i: Ошибка вызова clGetDeviceInfo(CL_DEVICE_MAX_WORK_GROUP_SIZE)", err);
		return;
	}
	// Получение числа ядер устройства на каждом из которых запускается одна группа рабочих элементов
	err = clGetDeviceInfo(devices[0], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), (void*)&units, 0);
	if (err != CL_SUCCESS) {
		printf("\n(timSort)Error %i: Ошибка вызова clGetDeviceInfo(CL_DEVICE_MAX_COMPUTE_UNITS)", err);
		return;
	}
	units /= 4;
	// Вычисление максимильного количества рабочих элементов, способных запустить на данном устройстве
	int SizeWorkMax = SizeGroup[0] * units;
	
	// Этап 1 - вычисление размера подмассивов, их число, число подмассивов с +1, числом рабочих элементов
	int nm = 0, narr = 1, sa, bufarrs = 1, sgrarr = 1;
	float fsa = 1;
	if (size_arr >= 16) {
		do {
			narr *= 2;
			if (narr <= SizeWorkMax) {
				SizeWork[0] = narr;
			}
			fsa = size_arr / narr;
		} while (fsa > 256); //=
		sa = floor(fsa);
		nm = size_arr - (narr * sa);
		if (narr <= SizeGroup[0]) {
			//SizeWork[0] = narr;
			SizeGroup[0] = narr;
			sgrarr = narr;
		}
		else {
			sgrarr = narr / (SizeWork[0] / SizeGroup[0]);
		}
		bufarrs = (narr / 2) * (sa + 1);
	}
	else {
		sa = size_arr;
		SizeWork[0] = 1;
		SizeGroup[0] = 1;
	}
	// Код сортировки для ядра графического процессора 
	const char* opencl_source = 
		// Функция сравнения:
		"bool Compare(@ el1, @ el2) {"
			"return (el2 № el1) ? true : false;"
		"}"
		// Функция перестановки элементов:
		"void Swap(__global @ *arr, int il, int ir) {"
			"if(il != ir) {"
				"@ bfr = arr[il];"
				"arr[il] = arr[ir];"
				"arr[ir] = bfr;"
			"}"
		"}"
		// Функция переворота подмассива:
		"void SwapArr(__global @ *arr, int intcrst, int intcrnd) {"
			"int iter = (intcrnd - intcrst + 1) / 2;"	
			"for (int j = 0; j < iter; j++) {"
				"Swap(arr, intcrst, intcrnd);"
				"intcrst++; intcrnd--;"
			"}"
		"}"
		// Основная функция сортировки:
		"__kernel void TimSort (__global @* arr, __global @* bufarr, __global int* lnch2stp) {"
			"#" //"int sa = sa, nm = nm, sgrarr = sgrarr;"
			
			"int indarr = (get_group_id(0) + 1) * sgrarr, stps = 2, sfa = 1;"
		"if (*lnch2stp == 0) {"
			// Этап 2 - сортировка вставкой с оптимизацией
			"int istarr = get_local_id(0) + (get_group_id(0) * sgrarr), ist, iend;"
			"while (istarr < indarr) {"
				"ist = istarr * sa;"
				"if (nm <= istarr) { ist += nm; iend = ist + (sa - 1); } else { ist += istarr; iend = ist + sa; }"
				//Сортировка вставками:
				// Оптимизация:
				"int istcr = ist, intcr = ist, intcrst, intcrnd;"
				"bool bstcr = true, bntcr = true, ntcr = false;"
				"for (int i = ist; i < iend; i++) {"
					"if (Compare(arr[i],arr[i+1]) == true) {"
						"if (bstcr) { istcr = i; }"
						"if (bntcr) { bntcr = false; }"
						"if (ntcr) { ntcr = false; SwapArr(arr, intcrst, intcrnd); i--; }"
					"} else {"
						"if (bstcr) { bstcr = false; }"
						"if (bntcr) { intcr = i; }"
						"if (!ntcr) { ntcr = true; intcrst = i; }"
						"intcrnd = i + 1;"
						"if (intcrnd == iend) { SwapArr(arr, intcrst, intcrnd); }"
					"}"
				"}"
				// Сортировка:
				"if ( !((istcr == iend) || (intcr == iend)) ) {"
					"for (int i = (istcr >= intcr) ? istcr : intcr; i < iend; i++) {"
						"if (Compare(arr[i],arr[i+1]) == false) {"
							"Swap(arr, i, (i + 1));"
							"for (int j = i; j > ist; j--) {"
								"if (Compare(arr[j],arr[j-1]) == true) {"
									"Swap(arr, j, (j - 1));"
								"} else { break;}"
							"}"
						"}"
					"}"
				"}"		
				"istarr += get_local_size(0);"
			"}"
			"barrier(CLK_GLOBAL_MEM_FENCE);"
		"} else {"
			"stps = *lnch2stp; sfa = stps / 2; sgrarr = $; indarr = sgrarr;"
		"}"
			// Этап 3 - сортировка слиянием с оптимизацией
			"int iarrs, iarrb, iarre, is1, ie1, is2, ie2;"
			"while (stps <= sgrarr) {"
				"iarrs = (get_local_id(0) * stps) + (get_group_id(0) * sgrarr);"
				"while (iarrs < indarr) {"
					"is1 = (iarrs * sa) + ((nm <= iarrs) ? nm : iarrs);"
					"iarre = iarrs + stps;"
					"ie2 = ((iarre * sa) - 1) + ((nm <= iarre) ? nm : iarre);"
					"iarrb = iarrs + sfa;"
					"is2 = (iarrb * sa) + ((nm <= iarrb) ? nm : iarrb);"
					"ie1 = is2 - 1;"
					// Сортировка слиянием:
					// Оптимизация:
					"if (Compare(arr[ie1],arr[is2]) == false) {"
						"int bufarrj = get_global_id(0) * sfa * (sa + 1);"
						"for (int i = is1, j = bufarrj; i <= ie1; i++, j++) { bufarr[j] = arr[i]; }"
						"if (Compare(arr[ie2],arr[is1]) == true) {"
							"int icp1 = is1 + (ie2 - is2 + 1);"
							"for (int j = bufarrj; icp1 <= ie2; j++, icp1++) {"
								"if (is2 <= ie2) {"
									"arr[is1] = arr[is2];"
									"is1++; is2++;"
								"}"
								"arr[icp1] = bufarr[j];"
							"}"
						"} else {"
							// Сортировка:
							"for (int i = is1, j = bufarrj, jnd = bufarrj + ie1 - is1; ; i++) {"
								"if (Compare(bufarr[j],arr[is2]) == true) {"
									"arr[i] = bufarr[j];"
									"if (j == jnd) { break; }"
									"j++;"
								"} else {"
									"arr[i] = arr[is2];"
									"if (is2 == ie2) {"
										"++i;"
										"for (; i <= ie2; i++, j++) { arr[i] = bufarr[j]; }"
										"break;"
									"}"
									"is2++;"
								"}"
							"}"
						"}"
					"}"
					"iarrs += (get_local_size(0) * stps);"
				"}"
				"sfa = stps;"
				"stps *= 2;"
				"barrier(CLK_GLOBAL_MEM_FENCE);"
			"}"
			"if ((get_global_id(0) == 0) && (*lnch2stp == 0)) { *lnch2stp = stps;}"
		"} \n";
	
	// Динамисечкое программирование кода ядра графического процессора
	int len = strlen(opencl_source);
	char* opencl_source_dinamic = new char[len + 200];
	int j = 0, typesize;
	for (int i = 0; i < len; i++)
	{
		if (!(strchr("@№#$", opencl_source[i])))
		{
			opencl_source_dinamic[j] = opencl_source[i]; j++;
		}
		else {
			if (opencl_source[i] == '@')
			{
				switch (type_elem) {
					case 0: { strcpy_s(&opencl_source_dinamic[j], 5, "bool"); j += 4; typesize = 1; break; }
					case 1: { strcpy_s(&opencl_source_dinamic[j], 5, "char"); j += 4; typesize = 1; break; }
					case 2: { strcpy_s(&opencl_source_dinamic[j], 6, "uchar"); j += 5; typesize = 1; break; }
					case 3: { strcpy_s(&opencl_source_dinamic[j], 6, "short"); j += 5; typesize = 2; break; }
					case 4: { strcpy_s(&opencl_source_dinamic[j], 7, "ushort"); j += 6; typesize = 2; break; }
					case 5: { strcpy_s(&opencl_source_dinamic[j], 4, "int"); j += 3; typesize = 4; break; }
					case 6: { strcpy_s(&opencl_source_dinamic[j], 5, "uint"); j += 4; typesize = 4; break; }
					case 7: { strcpy_s(&opencl_source_dinamic[j], 5, "long"); j += 4; typesize = 8; break; }
					case 8: { strcpy_s(&opencl_source_dinamic[j], 6, "ulong"); j += 5; typesize = 8; break; }
					case 9: { strcpy_s(&opencl_source_dinamic[j], 6, "float"); j += 5; typesize = 4; break; }
					case 10: { strcpy_s(&opencl_source_dinamic[j], 7, "double"); j += 6; typesize = 8; break; }
					default: break;
				}
			}
			if (opencl_source[i] == '№')
			{
				switch (type_sort) {
					case 1: { strcpy_s(&opencl_source_dinamic[j], 3, ">="); j += 2; break; }
					case 0: { strcpy_s(&opencl_source_dinamic[j], 3, "<="); j += 2; break; }
					default: break;
				}
			}
			if (opencl_source[i] == '#')
			{
				std::string str = "int sa = " + std::to_string(sa) + ", nm = " + std::to_string(nm) + ", sgrarr = " + std::to_string(sgrarr) + "; ";
				strcpy_s(&opencl_source_dinamic[j], str.size()+1, str.c_str());
				j += str.length();
			}
			if (opencl_source[i] == '$')
			{
				std::string str = std::to_string(narr);
				strcpy_s(&opencl_source_dinamic[j], str.size() + 1, str.c_str());
				j += str.length();
			}
		}
	}
	opencl_source_dinamic[j] = '\0';
	const char* opencl_source_const = opencl_source_dinamic;
	// Функция создания и получения программного объекта из context и кода сортировки
	cl_program opencl_program = clCreateProgramWithSource(*context, 1, &opencl_source_const, NULL, &arg_int);
	if (arg_int != CL_SUCCESS) {
		printf("\n(timSort)Error %i: Ошибка создания программного объекта", arg_int);
		return;
	}
	// Функция компиляции и сборки исполняемого файла из программного объекта
	err = clBuildProgram(opencl_program, 1, devices, NULL, func_cl_program, (void*)devices);
	if (err != CL_SUCCESS) {
		printf("\n(timSort)Error %i: Ошибка компиляции или сборки программы", err);
		return;
	}
	// Функция создания и возвращения объекта-ядра, ассоциированного с главной функцией ядра графического процессора
	cl_kernel kernel = clCreateKernel(opencl_program, "TimSort", &arg_int);
	if (arg_int != CL_SUCCESS) {
		printf("\n(timSort)Error %i: Ошибка создания объекта ядра", arg_int);
		return;
	}
	// Функция назначания аргумента(сортируеммый массив данных) для указанного ядра
	err = clSetKernelArg(kernel, 0, sizeof(arrays), (void*)arrays);
	if (err != CL_SUCCESS) {
		printf("\n(timSort)Error %i: Ошибка вызова clSetKernelArg", err);
		return;
	}
	// Функции создания объекта памяти(буферный массив) и назначение его аргументом для ядра 
	cl_mem mem_bufarr = clCreateBuffer(*context, CL_MEM_READ_WRITE, bufarrs * typesize, NULL, &arg_int);
	if (arg_int != CL_SUCCESS) {
		printf("\n(timSort)Error %i: Ошибка создания объекта памяти", arg_int);
		return;
	}
	err = clSetKernelArg(kernel, 1, sizeof(mem_bufarr), (void*)&mem_bufarr);
	if (err != CL_SUCCESS) {
		printf("\n(timSort)Error %i: Ошибка вызова clSetKernelArg", err);
		return;
	}
	// Функции создания объекта памяти(флаг следующего запуска), запись данных в него и назначение его аргументом для ядра
	cl_mem mem_lnch2stp = clCreateBuffer(*context, CL_MEM_READ_WRITE, sizeof(int), NULL, &arg_int);
	if (arg_int != CL_SUCCESS) {
		printf("\n(timSort)Error %i: Ошибка создания объекта памяти", arg_int);
		return;
	}
	int lnch2stp = 0;
	err = clEnqueueWriteBuffer(*command_queue, mem_lnch2stp, CL_TRUE, 0, sizeof(int), (const void*)&lnch2stp, 0, 0, 0);
	if (err != CL_SUCCESS) {
		printf("\n(timSort)Error %i: Ошибка вызова clEnqueueWriteBuffer", err);
		return;
	}
	err = clSetKernelArg(kernel, 2, sizeof(mem_lnch2stp), (void*)&mem_lnch2stp);
	if (err != CL_SUCCESS) {
		printf("\n(timSort)Error %i: Ошибка вызова clSetKernelArg", err);
		return;
	}
	// Первый запуск ядра и ожидание окончания вычислений
	err = clEnqueueNDRangeKernel(*command_queue, kernel, 1, NULL, SizeWork, SizeGroup, 0, 0, 0);
	if (err != CL_SUCCESS) {
		printf("\n(timSort)Error %i: Ошибка первого запуска ядра", err);
		return;
	}
	err = clFinish(*command_queue);
	if (err != CL_SUCCESS) {
		printf("\n(timSort)Error %i: Ошибка первого завершения ядра", err);
		return;
	}
	// Второй запуск ядра и ожидание окончания вычислений при условии, что было запущено несколько групп
	if ((SizeWork[0] / SizeGroup[0]) != 1) {
		SizeWork[0] = (SizeWork[0] / SizeGroup[0]) / 2;
		err = clEnqueueNDRangeKernel(*command_queue, kernel, 1, NULL, SizeWork, SizeWork, 0, 0, 0);
		if (err != CL_SUCCESS) {
			printf("\n(timSort)Error %i: Ошибка второго запуска ядра", err);
			return;
		}
		err = clFinish(*command_queue);
		if (err != CL_SUCCESS) {
			printf("\n(timSort)Error %i: Ошибка второго завершения ядра", err);
			return;
		}
	}
	// Освобождение OpenCL ресурсов
	clReleaseKernel(kernel);
	clReleaseProgram(opencl_program);
	clReleaseMemObject(mem_bufarr);
	clReleaseMemObject(mem_lnch2stp);
}

