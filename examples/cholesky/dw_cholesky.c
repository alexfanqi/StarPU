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

#include "dw_cholesky.h"
#include "dw_cholesky_models.h"

/*
 *	Some useful functions
 */

static struct starpu_task *create_task(starpu_tag_t id)
{
	struct starpu_task *task = starpu_task_create();
		task->cl_arg = NULL;
		task->use_tag = 1;
		task->tag_id = id;

	return task;
}

/*
 *	Create the codelets
 */

static starpu_codelet cl11 =
{
	.where = STARPU_CPU|STARPU_CUDA,
	.cpu_func = chol_cpu_codelet_update_u11,
#ifdef STARPU_USE_CUDA
	.cuda_func = chol_cublas_codelet_update_u11,
#endif
	.nbuffers = 1,
	.model = &chol_model_11
};

static struct starpu_task * create_task_11(starpu_data_handle dataA, unsigned k)
{
//	printf("task 11 k = %d TAG = %llx\n", k, (TAG11(k)));

	struct starpu_task *task = create_task(TAG11(k));
	
	task->cl = &cl11;

	/* which sub-data is manipulated ? */
	task->buffers[0].handle = starpu_data_get_sub_data(dataA, 2, k, k);
	task->buffers[0].mode = STARPU_RW;

	/* this is an important task */
	if (!noprio)
		task->priority = STARPU_MAX_PRIO;

	/* enforce dependencies ... */
	if (k > 0) {
		starpu_tag_declare_deps(TAG11(k), 1, TAG22(k-1, k, k));
	}

	return task;
}

static starpu_codelet cl21 =
{
	.where = STARPU_CPU|STARPU_CUDA,
	.cpu_func = chol_cpu_codelet_update_u21,
#ifdef STARPU_USE_CUDA
	.cuda_func = chol_cublas_codelet_update_u21,
#endif
	.nbuffers = 2,
	.model = &chol_model_21
};

static void create_task_21(starpu_data_handle dataA, unsigned k, unsigned j)
{
	struct starpu_task *task = create_task(TAG21(k, j));

	task->cl = &cl21;	

	/* which sub-data is manipulated ? */
	task->buffers[0].handle = starpu_data_get_sub_data(dataA, 2, k, k); 
	task->buffers[0].mode = STARPU_R;
	task->buffers[1].handle = starpu_data_get_sub_data(dataA, 2, k, j); 
	task->buffers[1].mode = STARPU_RW;

	if (!noprio && (j == k+1)) {
		task->priority = STARPU_MAX_PRIO;
	}

	/* enforce dependencies ... */
	if (k > 0) {
		starpu_tag_declare_deps(TAG21(k, j), 2, TAG11(k), TAG22(k-1, k, j));
	}
	else {
		starpu_tag_declare_deps(TAG21(k, j), 1, TAG11(k));
	}

	starpu_task_submit(task);
}

static starpu_codelet cl22 =
{
	.where = STARPU_CPU|STARPU_CUDA,
	.cpu_func = chol_cpu_codelet_update_u22,
#ifdef STARPU_USE_CUDA
	.cuda_func = chol_cublas_codelet_update_u22,
#endif
	.nbuffers = 3,
	.model = &chol_model_22
};

static void create_task_22(starpu_data_handle dataA, unsigned k, unsigned i, unsigned j)
{
//	printf("task 22 k,i,j = %d,%d,%d TAG = %llx\n", k,i,j, TAG22(k,i,j));

	struct starpu_task *task = create_task(TAG22(k, i, j));

	task->cl = &cl22;

	/* which sub-data is manipulated ? */
	task->buffers[0].handle = starpu_data_get_sub_data(dataA, 2, k, i); 
	task->buffers[0].mode = STARPU_R;
	task->buffers[1].handle = starpu_data_get_sub_data(dataA, 2, k, j); 
	task->buffers[1].mode = STARPU_R;
	task->buffers[2].handle = starpu_data_get_sub_data(dataA, 2, i, j); 
	task->buffers[2].mode = STARPU_RW;

	if (!noprio && (i == k + 1) && (j == k +1) ) {
		task->priority = STARPU_MAX_PRIO;
	}

	/* enforce dependencies ... */
	if (k > 0) {
		starpu_tag_declare_deps(TAG22(k, i, j), 3, TAG22(k-1, i, j), TAG21(k, i), TAG21(k, j));
	}
	else {
		starpu_tag_declare_deps(TAG22(k, i, j), 2, TAG21(k, i), TAG21(k, j));
	}

	starpu_task_submit(task);
}



/*
 *	code to bootstrap the factorization 
 *	and construct the DAG
 */

static void _dw_cholesky(starpu_data_handle dataA, unsigned nblocks)
{
	struct timeval start;
	struct timeval end;

	/* create a new codelet */
	sem_t sem;
	sem_init(&sem, 0, 0U);

	struct starpu_task *entry_task = NULL;

	/* create all the DAG nodes */
	unsigned i,j,k;

	gettimeofday(&start, NULL);

	for (k = 0; k < nblocks; k++)
	{
		struct starpu_task *task = create_task_11(dataA, k);
		/* we defer the launch of the first task */
		if (k == 0) {
			entry_task = task;
		}
		else {
			starpu_task_submit(task);
		}
		
		for (j = k+1; j<nblocks; j++)
		{
			create_task_21(dataA, k, j);

			for (i = k+1; i<nblocks; i++)
			{
				if (i <= j)
					create_task_22(dataA, k, i, j);
			}
		}
	}

	/* schedule the codelet */
	starpu_task_submit(entry_task);

	/* stall the application until the end of computations */
	starpu_tag_wait(TAG11(nblocks-1));

	starpu_data_unpartition(dataA, 0);

	gettimeofday(&end, NULL);


	double timing = (double)((end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec));
	fprintf(stderr, "Computation took (in ms)\n");
	printf("%2.2f\n", timing/1000);

	unsigned n = starpu_matrix_get_nx(dataA);

	double flop = (1.0f*n*n*n)/3.0f;
	fprintf(stderr, "Synthetic GFlops : %2.2f\n", (flop/timing/1000.0f));
}

void initialize_system(float **A, unsigned dim, unsigned pinned)
{
	starpu_init(NULL);
	
	starpu_helper_cublas_init();

	if (pinned)
	{
		starpu_data_malloc_pinned_if_possible((void **)A, (size_t)dim*dim*sizeof(float));
	} 
	else {
		*A = malloc(dim*dim*sizeof(float));
	}
}

void dw_cholesky(float *matA, unsigned size, unsigned ld, unsigned nblocks)
{
	starpu_data_handle dataA;

	/* monitor and partition the A matrix into blocks :
	 * one block is now determined by 2 unsigned (i,j) */
	starpu_matrix_data_register(&dataA, 0, (uintptr_t)matA, ld, size, size, sizeof(float));

	starpu_data_set_sequential_consistency_flag(dataA, 0);

	starpu_filter f;
		f.filter_func = starpu_vertical_block_filter_func;
		f.nchildren = nblocks;
		f.get_nchildren = NULL;
		f.get_child_ops = NULL;

	starpu_filter f2;
		f2.filter_func = starpu_block_filter_func;
		f2.nchildren = nblocks;
		f2.get_nchildren = NULL;
		f2.get_child_ops = NULL;

	starpu_map_filters(dataA, 2, &f, &f2);

	_dw_cholesky(dataA, nblocks);

	starpu_helper_cublas_shutdown();

	starpu_shutdown();
}

int main(int argc, char **argv)
{
	/* create a simple definite positive symetric matrix example
	 *
	 *	Hilbert matrix : h(i,j) = 1/(i+j+1)
	 * */

	parse_args(argc, argv);

	float *mat;

	mat = malloc(size*size*sizeof(float));
	initialize_system(&mat, size, pinned);

	unsigned i,j;
	for (i = 0; i < size; i++)
	{
		for (j = 0; j < size; j++)
		{
			mat[j +i*size] = (1.0f/(1.0f+i+j)) + ((i == j)?1.0f*size:0.0f);
			//mat[j +i*size] = ((i == j)?1.0f*size:0.0f);
		}
	}


#ifdef CHECK_OUTPUT
	printf("Input :\n");

	for (j = 0; j < size; j++)
	{
		for (i = 0; i < size; i++)
		{
			if (i <= j) {
				printf("%2.2f\t", mat[j +i*size]);
			}
			else {
				printf(".\t");
			}
		}
		printf("\n");
	}
#endif


	dw_cholesky(mat, size, size, nblocks);

#ifdef CHECK_OUTPUT
	printf("Results :\n");

	for (j = 0; j < size; j++)
	{
		for (i = 0; i < size; i++)
		{
			if (i <= j) {
				printf("%2.2f\t", mat[j +i*size]);
			}
			else {
				printf(".\t");
				mat[j+i*size] = 0.0f; // debug
			}
		}
		printf("\n");
	}

	fprintf(stderr, "compute explicit LLt ...\n");
	float *test_mat = malloc(size*size*sizeof(float));
	STARPU_ASSERT(test_mat);

	SSYRK("L", "N", size, size, 1.0f, 
				mat, size, 0.0f, test_mat, size);

	fprintf(stderr, "comparing results ...\n");
	for (j = 0; j < size; j++)
	{
		for (i = 0; i < size; i++)
		{
			if (i <= j) {
				printf("%2.2f\t", test_mat[j +i*size]);
			}
			else {
				printf(".\t");
			}
		}
		printf("\n");
	}
#endif

	return 0;
}
