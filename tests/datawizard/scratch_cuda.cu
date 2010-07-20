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

#include <stdio.h>
#include <starpu.h>

#define MAXNBLOCKS		32
#define MAXTHREADSPERBLOCK	128

static __global__ void increment_vector(unsigned *v, unsigned *tmp, int nx)
{
	const int tid = threadIdx.x + blockIdx.x*blockDim.x;
	const int nthreads = gridDim.x * blockDim.x;

	int i;
	for (i = tid; i < nx; i += nthreads)
	{
		v[i] = tmp[i] + 1;
	}
}

extern "C" void cuda_f(void *descr[], STARPU_ATTRIBUTE_UNUSED void *_args)
{
	unsigned *v = (unsigned *)STARPU_VECTOR_GET_PTR(descr[0]);
	unsigned *tmp = (unsigned *)STARPU_VECTOR_GET_PTR(descr[1]);

	unsigned nx = STARPU_VECTOR_GET_NX(descr[0]);
	size_t elemsize = STARPU_VECTOR_GET_ELEMSIZE(descr[0]);
	
	cudaMemcpy(tmp, v, nx*elemsize, cudaMemcpyDeviceToDevice);

	unsigned nblocks = 128;
	unsigned nthread_per_block = STARPU_MIN(MAXTHREADSPERBLOCK, (nx / nblocks));
	
	increment_vector<<<nblocks, nthread_per_block>>>(v, tmp, nx);
	cudaThreadSynchronize();
}
