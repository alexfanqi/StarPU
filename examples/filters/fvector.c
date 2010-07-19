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

#include <starpu.h>

#define NX    21
#define PARTS 3

void cpu_func(void *buffers[], void *cl_arg)
{
        unsigned i;
        int *factor = cl_arg;

        /* length of the vector */
        unsigned n = STARPU_GET_VECTOR_NX(buffers[0]);
        /* local copy of the vector pointer */
        int *val = (int *)STARPU_GET_VECTOR_PTR(buffers[0]);

        for (i = 0; i < n; i++)
                val[i] *= *factor;
}

int main(int argc, char **argv)
{
	unsigned i;
        int vector[NX];
        starpu_data_handle handle;
        int factor=1;

        starpu_codelet cl = {
                .where = STARPU_CPU,
                .cpu_func = cpu_func,
                .nbuffers = 1
        };

        for(i=0 ; i<NX ; i++) vector[i] = i;
        fprintf(stderr,"IN  Vector: ");
        for(i=0 ; i<NX ; i++) fprintf(stderr, "%d ", vector[i]);
        fprintf(stderr,"\n");

	starpu_init(NULL);

	/* Declare data to StarPU */
	starpu_vector_data_register(&handle, 0, (uintptr_t)vector, NX, sizeof(vector[0]));

        /* Partition the vector in PARTS sub-vectors */
	struct starpu_data_filter f =
	{
		.filter_func = starpu_block_filter_func_vector,
		.nchildren = PARTS,
		.get_nchildren = NULL,
		.get_child_ops = NULL
	};
	starpu_data_partition(handle, &f);

        /* Submit a task on each sub-vector */
	for (i=0; i<PARTS; i++)
	{
                starpu_data_handle sub_handle = starpu_data_get_sub_data(handle, 1, i);
                struct starpu_task *task = starpu_task_create();

                factor *= 10;
		task->buffers[0].handle = sub_handle;
		task->buffers[0].mode = STARPU_RW;
                task->cl = &cl;
                task->synchronous = 1;
                task->cl_arg = &factor;
                task->cl_arg_size = sizeof(factor);

		starpu_task_submit(task);
	}

        starpu_data_unregister(handle);
	starpu_shutdown();

        fprintf(stderr,"OUT Vector: ");
        for(i=0 ; i<NX ; i++) fprintf(stderr, "%d ", vector[i]);
        fprintf(stderr,"\n");

	return 0;
}
