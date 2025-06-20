/*
 * File: IntroSort.cpp
 * Author: Egor Zhelamskiy
 * Date: June 20, 2025
 * Description: Introsort* for OpenCL.
 * * - without pyramid sorting.
 */
#include "ParSortLib.h"

#include <ctime>

// Функция получения логов компилятора и сборщика программы
void __stdcall introsrt_func_cl_program(cl_program program, void* user_data)
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

void introSort(cl_device_id* devices, cl_context* context, cl_command_queue* command_queue, cl_mem* arrays, cl_uint size_arr, cl_uint type_elem, cl_uint type_sort, cl_uint size_inst, cl_uint avrg_elem) {
	if (size_arr <= 2) {
		printf("\n(introSort)Error: Маленький размер массива (size_arr <= 2)\n");
		return;
	}
	if (type_sort > 1) {
		printf("\n(introSort)Error: This type_sort value was not found\n");
		return;
	}
	if (avrg_elem > 4) {
		printf("\n(introSort)Error: This avrg_elem value was not found\n");
		return;
	}
	cl_int err, arg_int;
	cl_uint units;
	size_t SizeWork[3]{}, SizeGroup[3]{};
	// Получение информации о максимальном размере группы рабочих элементов
	err = clGetDeviceInfo(devices[0], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), (void*)&SizeGroup, NULL);
	if (err != CL_SUCCESS) {
		printf("\n(introSort)Error %i: Ошибка вызова clGetDeviceInfo(CL_DEVICE_MAX_WORK_GROUP_SIZE)", err);
		return;
	}
	// Получение числа ядер устройства на каждом из которых запускается одна группа рабочих элементов
	err = clGetDeviceInfo(devices[0], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), (void*)&units, 0);
	if (err != CL_SUCCESS) {
		printf("\n(introSort)Error %i: Ошибка вызова clGetDeviceInfo(CL_DEVICE_MAX_COMPUTE_UNITS)", err);
		return;
	}
	// Вычисление максимильного количества рабочих элементов, способных запустить на данном устройстве
	int SizeWorkMax = SizeGroup[0] * units;
	// Вычисление начальных параметров, необходимых для дальнейшей работы сортировки
	srand(time(0));
	int glrnd = rand();
	int lnch2stp = 0;
	int szbfarr = SizeGroup[0] * 4;
	int* bfarr = new int[szbfarr] {};
	bfarr[1] = (size_arr - 1);
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
		//Сортировка вставками:
		"void SortInsert(__global @ *arr, int ist, int iend){"
			// Оптимизация:
			"int istcr = ist, intcr = ist, intcrst, intcrnd;"
			"bool bstcr = true, bntcr = true, ntcr = false;"
			"@ buff;"
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
		"}"
		// Функция псевдо-случайного выбора опорного элемента:
		"int rndf(int seed, int min, int max) {"
			"ulong next = (seed * 1103515245 + 12345) / #;"
			"return  (next % (max - min + 1)) + min;"
		"}"
		// Основная функция сортировки:
		"__kernel void IntroSort(__global @* arr, __global int* bfarr, __global int* lnch2stp) {"
			"int stps, stpsz = get_local_size(0) + 1, szsbarr = 1, idgrp = get_group_id(0), idlcl = get_local_id(0), idlcl4 = idlcl * 4;"
			"int il1 = -1, ir1 = -1, il2 = -1, ir2 = -1;"
			"int iavrg;"
			"__local int flgcmplt;"
			"__local int bfgrarr[$];"
			"if (*lnch2stp) {"
				"szsbarr = (get_local_size(0) * 2);"
			"}"
			"while (idgrp < szsbarr) {"
				// Получение первоначальных размеров подмассива группы
				"if (idlcl == 0) {"
					"bfgrarr[0] = bfarr[idgrp * 2]; bfgrarr[1] = bfarr[(idgrp * 2) + 1];"
				"}"
				"stps = 1;"
				"while (stps < stpsz) {"
					// Проверка на уже отсортированность подмассива группы
					"if(get_local_id(0) == 0){"
						"flgcmplt = 0;"
						"for(int x = 1; x < (stps * 2); x += 2) {"
							"if(bfgrarr[x] == -1) {flgcmplt += 1;}"
						"}"
						"if(flgcmplt != stps) { flgcmplt = 0; }"
					"}"
					"barrier(CLK_GLOBAL_MEM_FENCE);"
					"if(flgcmplt == stps) { break; }"
					"if (idlcl < stps) {"
						"il1 = bfgrarr[idlcl * 2]; ir2 = bfgrarr[(idlcl * 2) + 1];"
						// Выполнение обязательной сортировки вставкой при достижении окончания разделения подмассивов
						"if ((stps == get_local_size(0)) && (*lnch2stp)) {"
							"SortInsert(arr, il1, ir2);"
						"}else {"
							"if (ir2 != -1) {"
								// Сортировка вставкой при достижении определенного размера подмассива
								"if ((ir2 - il1 + 1) <= ~) {"
									"SortInsert(arr, il1, ir2);"
									"ir1 = -1; ir2 = -1;"
								"}else {"
									// Быстрая сортировка:
									//Выбор опорного(среднего) элемента:
									"iavrg = .;"
									"Swap(arr, ir2, iavrg);"
									"iavrg = ir2;"
									//Перестановка первая:
									"int i = il1, j = (ir2 - 1);"
									"while (i < j) {"
										"if (!(Compare(arr[i], arr[iavrg]))) {"
											"while (j > i) {"
												"if (Compare(arr[j], arr[iavrg])) {"
													"Swap(arr, j, i);"
													"j -= 1;"
													"break;"
												"}"
												"j -= 1;"
											"}"
										"}"
										"i += 1;"
									"}"
									"if (!(Compare(arr[j], arr[iavrg]))) {"
										"Swap(arr, iavrg, j);"
										"iavrg = j;"
									"}else {"
										"if (j != (ir2 - 1)) {"
											"Swap(arr, iavrg, (j + 1));"
											"iavrg = (j + 1);"
										"}"
									"}"
									"il2 = (iavrg == ir2) ? ir2 : (iavrg + 1);"
									"if ((ir2 - il2 + 1) == 1) {"
										"ir2 = -1;"
									"}"
									// Перестановка вторая (над левым массивом):
									"if ((iavrg - il1) < 2) {"
										"ir1 = -1;"
									"}else {"
										"i = il1, j = iavrg - 1;"
										"while (i < j) {"
											"if (arr[i] == arr[iavrg]) {"
												"while (j > i) {"
													"if (!(arr[j] == arr[iavrg])) {"
														"Swap(arr, j, i);"
														"j -= 1;"
														"break;"
													"}"
													"j -= 1;"
												"}"
											"}"
											"i += 1;"
										"}"
										"if (arr[j] == arr[iavrg]) {"
											"iavrg = j;"
										"}else {"
											"if (j != (iavrg - 1)) {"
												"iavrg = (j + 1);"
											"}"
										"}"
										"ir1 = (iavrg == il1) ? il1 : (iavrg - 1);"
										"if ((ir1 - il1 + 1) == 1) {"
											"ir1 = -1;"
										"}"
									"}"
								"}"
							"}else {"
								"ir1 = -1; ir2 = -1;"
							"}"
							// Запись идентификаторов подмассивов:
							"bfgrarr[idlcl4] = il1; bfgrarr[idlcl4 + 1] = ir1; bfgrarr[idlcl4 + 2] = il2; bfgrarr[idlcl4 + 3] = ir2;"
						"}"
					"}"
					"barrier(CLK_GLOBAL_MEM_FENCE);"
					"stps *= 2;"
				"}"
				"idgrp += get_num_groups(0);"
			"}"
			// Запись индексов подмассивов в глобальный буферный массив
			"barrier(CLK_GLOBAL_MEM_FENCE);"
			"if ((get_global_id(0) == 0) && (*lnch2stp == 0)) {"
				"*lnch2stp = 1;"
				"for (int i = 0; i < $; i++) {"
					"bfarr[i] = bfgrarr[i];"
				"}"
			"}"
		"} \n";

	// Динамисечкое программирование кода ядра графического процессора
	int len = strlen(opencl_source);
	char* opencl_source_dinamic = new char[len + 300];
	int j = 0, typesize;
	for (int i = 0; i < len; i++)
	{
		if (!(strchr("@№#$~.", opencl_source[i])))
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
			if (opencl_source[i] == '.')
			{
				switch (avrg_elem) {
				case 0: { strcpy_s(&opencl_source_dinamic[j], 47, "rndf((get_global_id(0) + il1 + ir2), il1, ir2)"); j += 46; break; }
				case 1: { strcpy_s(&opencl_source_dinamic[j], 4, "il1"); j += 3; break; }
				case 2: { strcpy_s(&opencl_source_dinamic[j], 4, "ir2"); j += 3; break; }
				case 3: { strcpy_s(&opencl_source_dinamic[j], 12, "(ir2+il1)/2"); j += 11; break; }
				default: break;
				}
			}
			if (opencl_source[i] == '#')
			{
				std::string str = std::to_string(glrnd);
				strcpy_s(&opencl_source_dinamic[j], str.size() + 1, str.c_str());
				j += str.length();
			}
			if (opencl_source[i] == '$')
			{
				std::string str = std::to_string(SizeGroup[0] * 4);
				strcpy_s(&opencl_source_dinamic[j], str.size() + 1, str.c_str());
				j += str.length();
			}
			if (opencl_source[i] == '~')
			{
				std::string str = std::to_string(size_inst);
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
		printf("\n(introSort)Error %i: Ошибка создания программного объекта", arg_int);
		return;
	}
	// Функция компиляции и сборки исполняемого файла из программного объекта
	err = clBuildProgram(opencl_program, 1, devices, NULL, introsrt_func_cl_program, (void*)devices);
	if (err != CL_SUCCESS) {
		printf("\n(introSort)Error %i: Ошибка компиляции или сборки программы", err);
		return;
	}
	// Функция создания и возвращения объекта-ядра, ассоциированного с главной функцией ядра графического процессора
	cl_kernel kernel = clCreateKernel(opencl_program, "IntroSort", &arg_int);
	if (arg_int != CL_SUCCESS) {
		printf("\n(introSort)Error %i: Ошибка создания объекта ядра", arg_int);
		return;
	}
	// Функция назначания аргумента(сортируеммый массив данных) для указанного ядра
	err = clSetKernelArg(kernel, 0, sizeof(arrays), (void*)arrays);
	if (err != CL_SUCCESS) {
		printf("\n(introSort)Error %i: Ошибка вызова clSetKernelArg", err);
		return;
	}
	// Функции создания объекта памяти(буферный массив для быстрой сортировки), запись данных в него и назначение его аргументом для ядра 
	cl_mem mem_bfarr = clCreateBuffer(*context, CL_MEM_READ_WRITE, szbfarr * sizeof(int), NULL, &arg_int);
	if (arg_int != CL_SUCCESS) {
		printf("\n(introSort)Error %i: Ошибка создания объекта памяти", arg_int);
		return;
	}
	err = clEnqueueWriteBuffer(*command_queue, mem_bfarr, CL_TRUE, 0, szbfarr * sizeof(int), (const void*)bfarr, 0, 0, 0);
	if (err != CL_SUCCESS) {
		printf("\n(introSort)Error %i: Ошибка вызова clEnqueueWriteBuffer", err);
		return;
	}
	err = clSetKernelArg(kernel, 1, sizeof(mem_bfarr), (void*)&mem_bfarr);
	if (err != CL_SUCCESS) {
		printf("\n(introSort)Error %i: Ошибка вызова clSetKernelArg", err);
		return;
	}
	// Функции создания объекта памяти(флаг следующего запуска), запись данных в него и назначение его аргументом для ядра 
	cl_mem mem_lnch2stp = clCreateBuffer(*context, CL_MEM_READ_WRITE, sizeof(int), NULL, &arg_int);
	if (arg_int != CL_SUCCESS) {
		printf("\n(introSort)Error %i: Ошибка создания объекта памяти", arg_int);
		return;
	}
	err = clEnqueueWriteBuffer(*command_queue, mem_lnch2stp, CL_TRUE, 0, sizeof(int), (const void*)&lnch2stp, 0, 0, 0);
	if (err != CL_SUCCESS) {
		printf("\n(introSort)Error %i: Ошибка вызова clEnqueueWriteBuffer", err);
		return;
	}
	err = clSetKernelArg(kernel, 2, sizeof(mem_lnch2stp), (void*)&mem_lnch2stp);
	if (err != CL_SUCCESS) {
		printf("\n(introSort)Error %i: Ошибка вызова clSetKernelArg", err);
		return;
	}
	// Первый запуск ядра и ожидание окончания вычислений
	SizeWork[0] = SizeGroup[0];
	err = clEnqueueNDRangeKernel(*command_queue, kernel, 1, NULL, SizeWork, SizeGroup, 0, 0, 0);
	if (err != CL_SUCCESS) {
		printf("\n(introSort)Error %i: Ошибка первого запуска ядра", err);
		return;
	}
	err = clFinish(*command_queue);
	if (err != CL_SUCCESS) {
		printf("\n(introSort)Error %i: Ошибка первого завершения ядра", err);
		return;
	}
	// Получение данных из объекта памяти(буферный массив для быстрой сортировки) и вычисление необходимости второго запуска
	err = clEnqueueReadBuffer(*command_queue, mem_bfarr, CL_TRUE, 0, szbfarr * sizeof(int), (void*)bfarr, 0, 0, 0);
	if (err != CL_SUCCESS) {
		printf("\n(introSort)Error %i: Ошибка clEnqueueReadBuffer", err);
		return;
	}
	bool is2stp = false;
	for (int i = 1; i < szbfarr; i += 2) {
		if (bfarr[i] != -1) {
			is2stp = true;
			break;
		}
	}
	// Второй запуск ядра и ожидание окончания вычислений при выполнении условий
	if (is2stp) {
		SizeWork[0] = SizeGroup[0] * units;
		err = clEnqueueNDRangeKernel(*command_queue, kernel, 1, NULL, SizeWork, SizeGroup, 0, 0, 0);
		if (err != CL_SUCCESS) {
			printf("\n(introSort)Error %i: Ошибка второго запуска ядра", err);
			return;
		}
		err = clFinish(*command_queue);
		if (err != CL_SUCCESS) {
			printf("\n(introSort)Error %i: Ошибка второго завершения ядра", err);
			return;
		}
	}
	// Освобождение OpenCL ресурсов
	clReleaseKernel(kernel);
	clReleaseProgram(opencl_program);
	clReleaseMemObject(mem_bfarr);
	clReleaseMemObject(mem_lnch2stp);
}

