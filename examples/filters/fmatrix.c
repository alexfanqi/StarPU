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

#define NX    5
#define NY    4
#define PARTS 2

void cpu_func(void *buffers[], void *cl_arg)
{
        unsigned i, j;
        int *factor = cl_arg;

        /* length of the matrix */
        unsigned nx = STARPU_GET_MATRIX_NX(buffers[0]);
        unsigned ny = STARPU_GET_MATRIX_NY(buffers[0]);
        unsigned ld = STARPU_GET_MATRIX_LD(buffers[0]);
        /* local copy of the matrix pointer */
        int *val = (int *)STARPU_GET_MATRIX_PTR(buffers[0]);

        for(j=0; j<ny ; j++) {
                for(i=0; i<nx ; i++)
                        val[(j*ld)+i] = *factor;
        }
}

int main(int argc, char **argv)
{
	unsigned i, j, n=1;
        int matrix[NX*NY];

        fprintf(stderr,"IN  Matrix: \n");
        for(j=0 ; j<NY ; j++) {
                for(i=0 ; i<NX ; i++) {
                        matrix[(j*NX)+i] = n++;
                        fprintf(stderr, "%2d ", matrix[(j*NX)+i]);
                }
                fprintf(stderr,"\n");
        }
        fprintf(stderr,"\n");

        starpu_data_handle handle;
        starpu_codelet cl = {
                .where = STARPU_CPU,
                .cpu_func = cpu_func,
                .nbuffers = 1
        };
        starpu_init(NULL);

	/* Declare data to StarPU */
	starpu_matrix_data_register(&handle, 0, (uintptr_t)matrix, NX, NX, NY, sizeof(matrix[0]));

        /* Partition the matrix in PARTS sub-matrices */
	struct starpu_data_filter f =
	{
		.filter_func = starpu_block_filter_func,
		.nchildren = PARTS,
		.get_nchildren = NULL,
		.get_child_ops = NULL
	};
	starpu_data_partition(handle, &f);

        /* Submit a task on each sub-vector */
	for (i=0; i<PARTS; i++)
	{
                struct starpu_task *task = starpu_task_create();
                int factor = i;
		task->buffers[0].handle = starpu_data_get_sub_data(handle, 1, i);
		task->buffers[0].mode = STARPU_RW;
                task->cl = &cl;
                task->synchronous = 1;
                task->cl_arg = &factor;
                task->cl_arg_size = sizeof(factor);
		starpu_task_submit(task);
	}

        /* Unpartition the data, unregister it from StarPU and shutdown */
	starpu_data_unpartition(handle, 0);
        starpu_data_unregister(handle);
	starpu_shutdown();

        /* Print result matrix */
        fprintf(stderr,"OUT Matrix: \n");
        for(j=0 ; j<NY ; j++) {
                for(i=0 ; i<NX ; i++) {
                        fprintf(stderr, "%2d ", matrix[(j*NX)+i]);
                }
                fprintf(stderr,"\n");
        }
        fprintf(stderr,"\n");

	return 0;
}
