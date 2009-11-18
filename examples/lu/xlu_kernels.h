/*
 * StarPU
 * Copyright (C) INRIA 2008-2009 (see AUTHORS file)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU Lesser General Public License in COPYING.LGPL for more details.
 */

#ifndef __XLU_KERNELS_H__
#define __XLU_KERNELS_H__

#include <starpu.h>

#define str(s) #s
#define xstr(s)        str(s)
#define STARPU_LU_STR(name)  xstr(STARPU_LU(name))

void STARPU_LU(cpu_pivot)(starpu_data_interface_t *descr, void *_args);
void STARPU_LU(cpu_u11_pivot)(starpu_data_interface_t *descr, void *_args);
void STARPU_LU(cpu_u11)(starpu_data_interface_t *descr, void *_args);
void STARPU_LU(cpu_u12)(starpu_data_interface_t *descr, void *_args);
void STARPU_LU(cpu_u21)(starpu_data_interface_t *descr, void *_args);
void STARPU_LU(cpu_u22)(starpu_data_interface_t *descr, void *_args);

#ifdef USE_CUDA
void STARPU_LU(cublas_pivot)(starpu_data_interface_t *descr, void *_args);
void STARPU_LU(cublas_u11_pivot)(starpu_data_interface_t *descr, void *_args);
void STARPU_LU(cublas_u11)(starpu_data_interface_t *descr, void *_args);
void STARPU_LU(cublas_u12)(starpu_data_interface_t *descr, void *_args);
void STARPU_LU(cublas_u21)(starpu_data_interface_t *descr, void *_args);
void STARPU_LU(cublas_u22)(starpu_data_interface_t *descr, void *_args);
#endif

#endif // __XLU_KERNELS_H__