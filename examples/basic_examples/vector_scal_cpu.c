/*
 * StarPU
 * Copyright (C) INRIA 2008-2010 (see AUTHORS file)
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

/*
 * This example complements vector_scale.c: here we implement a CPU version.
 */

#include <starpu.h>

/* This kernel takes a buffer and scales it by a constant factor */
void scal_cpu_func(void *buffers[], void *cl_arg)
{
	unsigned i;
	float *factor = cl_arg;

	/* 
	 * The "buffers" array matches the task->buffers array: for instance
	 * task->buffers[0].handle is a handle that corresponds to a data with
	 * vector "interface", so that the first entry of the array in the
	 * codelet  is a pointer to a structure describing such a vector (ie.
	 * struct starpu_vector_interface_s *). Here, we therefore manipulate
	 * the buffers[0] element as a vector: nx gives the number of elements
	 * in the array, ptr gives the location of the array (that was possibly
	 * migrated/replicated), and elemsize gives the size of each elements.
	 */

	starpu_vector_interface_t *vector = buffers[0];

	/* length of the vector */
	unsigned n = STARPU_GET_VECTOR_NX(vector);

	/* get a pointer to the local copy of the vector : note that we have to
	 * cast it in (float *) since a vector could contain any type of
	 * elements so that the .ptr field is actually a uintptr_t */
	float *val = (float *)STARPU_GET_VECTOR_PTR(vector);

	/* scale the vector */
	for (i = 0; i < n; i++)
		val[i] *= *factor;
}

