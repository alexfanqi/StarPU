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

#include "xlu.h"
#include "xlu_kernels.h"

#define TAG11(k)	((starpu_tag_t)( (1ULL<<60) | (unsigned long long)(k)))
#define TAG12(k,i)	((starpu_tag_t)(((2ULL<<60) | (((unsigned long long)(k))<<32)	\
					| (unsigned long long)(i))))
#define TAG21(k,j)	((starpu_tag_t)(((3ULL<<60) | (((unsigned long long)(k))<<32)	\
					| (unsigned long long)(j))))
#define TAG22(k,i,j)	((starpu_tag_t)(((4ULL<<60) | ((unsigned long long)(k)<<32) 	\
					| ((unsigned long long)(i)<<16)	\
					| (unsigned long long)(j))))
#define PIVOT(k,i)	((starpu_tag_t)(((5ULL<<60) | (((unsigned long long)(k))<<32)	\
					| (unsigned long long)(i))))

static unsigned no_prio = 0;




/*
 *	Construct the DAG
 */

static struct starpu_task *create_task(starpu_tag_t id)
{
	struct starpu_task *task = starpu_task_create();
		task->cl_arg = NULL;

	task->use_tag = 1;
	task->tag_id = id;

	return task;
}


static struct starpu_perfmodel_t STARPU_LU(model_pivot) = {
	.type = STARPU_HISTORY_BASED,
#ifdef ATLAS
	.symbol = STARPU_LU_STR(lu_model_pivot_atlas)
#elif defined(GOTO)
	.symbol = STARPU_LU_STR(lu_model_pivot_goto)
#else
	.symbol = STARPU_LU_STR(lu_model_pivot)
#endif
};

static starpu_codelet cl_pivot = {
	.where = STARPU_CPU|STARPU_CUDA,
	.cpu_func = STARPU_LU(cpu_pivot),
#ifdef USE_CUDA
	.cuda_func = STARPU_LU(cublas_pivot),
#endif
	.nbuffers = 1,
	.model = &STARPU_LU(model_pivot)
};

static void create_task_pivot(starpu_data_handle *dataAp, unsigned nblocks,
					struct piv_s *piv_description,
					unsigned k, unsigned i,
					starpu_data_handle (* get_block)(starpu_data_handle *, unsigned, unsigned, unsigned))
{
	struct starpu_task *task = create_task(PIVOT(k, i));

	task->cl = &cl_pivot;

	/* which sub-data is manipulated ? */
	task->buffers[0].handle = get_block(dataAp, nblocks, k, i);
	task->buffers[0].mode = STARPU_RW;

	task->cl_arg = &piv_description[k];

	/* this is an important task */
	if (!no_prio && (i == k+1))
		task->priority = STARPU_MAX_PRIO;

	/* enforce dependencies ... */
	if (k == 0) {
		starpu_tag_declare_deps(PIVOT(k, i), 1, TAG11(k));
	}
	else 
	{
		if (i > k) {
			starpu_tag_declare_deps(PIVOT(k, i), 2, TAG11(k), TAG22(k-1, i, k));
		}
		else {
			starpu_tag_t *tags = malloc((nblocks - k)*sizeof(starpu_tag_t));
			
			tags[0] = TAG11(k);
			unsigned ind, ind2;
			for (ind = k + 1, ind2 = 0; ind < nblocks; ind++, ind2++)
			{
				tags[1 + ind2] = TAG22(k-1, ind, k);
			}

			/* perhaps we could do better ... :/  */
			starpu_tag_declare_deps_array(PIVOT(k, i), (nblocks-k), tags);
		}
	}

	starpu_submit_task(task);
}

static struct starpu_perfmodel_t STARPU_LU(model_11_pivot) = {
	.type = STARPU_HISTORY_BASED,
#ifdef ATLAS
	.symbol = STARPU_LU_STR(lu_model_11_pivot_atlas)
#elif defined(GOTO)
	.symbol = STARPU_LU_STR(lu_model_11_pivot_goto)
#else
	.symbol = STARPU_LU_STR(lu_model_11_pivot)
#endif
};

static starpu_codelet cl11_pivot = {
	.where = STARPU_CPU|STARPU_CUDA,
	.cpu_func = STARPU_LU(cpu_u11_pivot),
#ifdef USE_CUDA
	.cuda_func = STARPU_LU(cublas_u11_pivot),
#endif
	.nbuffers = 1,
	.model = &STARPU_LU(model_11_pivot)
};

static struct starpu_task *create_task_11_pivot(starpu_data_handle *dataAp, unsigned nblocks,
					unsigned k, struct piv_s *piv_description,
					starpu_data_handle (* get_block)(starpu_data_handle *, unsigned, unsigned, unsigned))
{
	struct starpu_task *task = create_task(TAG11(k));

	task->cl = &cl11_pivot;

	task->cl_arg = &piv_description[k];

	/* which sub-data is manipulated ? */
	task->buffers[0].handle = get_block(dataAp, nblocks, k, k);
	task->buffers[0].mode = STARPU_RW;

	/* this is an important task */
	if (!no_prio)
		task->priority = STARPU_MAX_PRIO;

	/* enforce dependencies ... */
	if (k > 0) {
		starpu_tag_declare_deps(TAG11(k), 1, TAG22(k-1, k, k));
	}

	return task;
}

static struct starpu_perfmodel_t STARPU_LU(model_12) = {
	.type = STARPU_HISTORY_BASED,
#ifdef ATLAS
	.symbol = STARPU_LU_STR(lu_model_12_atlas)
#elif defined(GOTO)
	.symbol = STARPU_LU_STR(lu_model_12_goto)
#else
	.symbol = STARPU_LU_STR(lu_model_12)
#endif
};

static starpu_codelet cl12 = {
	.where = STARPU_CPU|STARPU_CUDA,
	.cpu_func = STARPU_LU(cpu_u12),
#ifdef USE_CUDA
	.cuda_func = STARPU_LU(cublas_u12),
#endif
	.nbuffers = 2,
	.model = &STARPU_LU(model_12)
};

static void create_task_12(starpu_data_handle *dataAp, unsigned nblocks, unsigned k, unsigned j,
		starpu_data_handle (* get_block)(starpu_data_handle *, unsigned, unsigned, unsigned))
{
//	printf("task 12 k,i = %d,%d TAG = %llx\n", k,i, TAG12(k,i));

	struct starpu_task *task = create_task(TAG12(k, j));
	
	task->cl = &cl12;

	task->cl_arg = (void *)(task->tag_id);

	/* which sub-data is manipulated ? */
	task->buffers[0].handle = get_block(dataAp, nblocks, k, k);
	task->buffers[0].mode = STARPU_R;
	task->buffers[1].handle = get_block(dataAp, nblocks, j, k);
	task->buffers[1].mode = STARPU_RW;

	if (!no_prio && (j == k+1)) {
		task->priority = STARPU_MAX_PRIO;
	}

	/* enforce dependencies ... */
#if 0
	starpu_tag_declare_deps(TAG12(k, i), 1, PIVOT(k, i));
#endif
	if (k > 0) {
		starpu_tag_declare_deps(TAG12(k, j), 2, TAG11(k), TAG22(k-1, k, j));
	}
	else {
		starpu_tag_declare_deps(TAG12(k, j), 1, TAG11(k));
	}

	starpu_submit_task(task);
}

static struct starpu_perfmodel_t STARPU_LU(model_21) = {
	.type = STARPU_HISTORY_BASED,
#ifdef ATLAS
	.symbol = STARPU_LU_STR(lu_model_21_atlas)
#elif defined(GOTO)
	.symbol = STARPU_LU_STR(lu_model_21_goto)
#else
	.symbol = STARPU_LU_STR(lu_model_21)
#endif
};

static starpu_codelet cl21 = {
	.where = STARPU_CPU|STARPU_CUDA,
	.cpu_func = STARPU_LU(cpu_u21),
#ifdef USE_CUDA
	.cuda_func = STARPU_LU(cublas_u21),
#endif
	.nbuffers = 2,
	.model = &STARPU_LU(model_21)
};

static void create_task_21(starpu_data_handle *dataAp, unsigned nblocks, unsigned k, unsigned i,
				starpu_data_handle (* get_block)(starpu_data_handle *, unsigned, unsigned, unsigned))
{
	struct starpu_task *task = create_task(TAG21(k, i));

	task->cl = &cl21;
	
	/* which sub-data is manipulated ? */
	task->buffers[0].handle = get_block(dataAp, nblocks, k, k); 
	task->buffers[0].mode = STARPU_R;
	task->buffers[1].handle = get_block(dataAp, nblocks, k, i); 
	task->buffers[1].mode = STARPU_RW;

	if (!no_prio && (i == k+1)) {
		task->priority = STARPU_MAX_PRIO;
	}

	task->cl_arg = (void *)(task->tag_id);

	/* enforce dependencies ... */
	starpu_tag_declare_deps(TAG21(k, i), 1, PIVOT(k, i));
#if 0
	if (k > 0) {
		starpu_tag_declare_deps(TAG21(k, i), 3, TAG11(k), TAG22(k-1, k, i), PIVOT(k, i));
	}
	else {
		starpu_tag_declare_deps(TAG21(k, i), 2, TAG11(k), PIVOT(k, i));
	}
#endif

	starpu_submit_task(task);
}

static struct starpu_perfmodel_t STARPU_LU(model_22) = {
	.type = STARPU_HISTORY_BASED,
#ifdef ATLAS
	.symbol = STARPU_LU_STR(lu_model_22_atlas)
#elif defined(GOTO)
	.symbol = STARPU_LU_STR(lu_model_22_goto)
#else
	.symbol = STARPU_LU_STR(lu_model_22)
#endif
};

static starpu_codelet cl22 = {
	.where = STARPU_CPU|STARPU_CUDA,
	.cpu_func = STARPU_LU(cpu_u22),
#ifdef USE_CUDA
	.cuda_func = STARPU_LU(cublas_u22),
#endif
	.nbuffers = 3,
	.model = &STARPU_LU(model_22)
};

static void create_task_22(starpu_data_handle *dataAp, unsigned nblocks, unsigned k, unsigned i, unsigned j,
				starpu_data_handle (* get_block)(starpu_data_handle *, unsigned, unsigned, unsigned))
{
//	printf("task 22 k,i,j = %d,%d,%d TAG = %llx\n", k,i,j, TAG22(k,i,j));

	struct starpu_task *task = create_task(TAG22(k, i, j));

	task->cl = &cl22;

	task->cl_arg = (void *)(task->tag_id);

	/* which sub-data is manipulated ? */
	task->buffers[0].handle = get_block(dataAp, nblocks, k, i); /* produced by TAG21(k, i) */
	task->buffers[0].mode = STARPU_R;
	task->buffers[1].handle = get_block(dataAp, nblocks, j, k); /* produced by TAG12(k, j) */ 
	task->buffers[1].mode = STARPU_R;
	task->buffers[2].handle = get_block(dataAp, nblocks, j, i);  /* produced by TAG22(k-1, i, j) */
	task->buffers[2].mode = STARPU_RW;

	if (!no_prio &&  (i == k + 1) && (j == k +1) ) {
		task->priority = STARPU_MAX_PRIO;
	}

	/* enforce dependencies ... */
	if (k > 0) {
		starpu_tag_declare_deps(TAG22(k, i, j), 3, TAG22(k-1, i, j), TAG12(k, j), TAG21(k, i));
	}
	else {
		starpu_tag_declare_deps(TAG22(k, i, j), 2, TAG12(k, j), TAG21(k, i));
	}

	starpu_submit_task(task);
}

/*
 *	code to bootstrap the factorization 
 */

static double dw_codelet_facto_pivot(starpu_data_handle *dataAp,
					struct piv_s *piv_description,
					unsigned nblocks,
					starpu_data_handle (* get_block)(starpu_data_handle *, unsigned, unsigned, unsigned))
{
	struct timeval start;
	struct timeval end;

	struct starpu_task *entry_task = NULL;

	/* create all the DAG nodes */
	unsigned i,j,k;

	for (k = 0; k < nblocks; k++)
	{
		struct starpu_task *task = create_task_11_pivot(dataAp, nblocks, k, piv_description, get_block);

		/* we defer the launch of the first task */
		if (k == 0) {
			entry_task = task;
		}
		else {
			starpu_submit_task(task);
		}

		for (i = 0; i < nblocks; i++)
		{
			if (i != k)
				create_task_pivot(dataAp, nblocks, piv_description, k, i, get_block);
		}
	
		for (i = k+1; i<nblocks; i++)
		{
			create_task_12(dataAp, nblocks, k, i, get_block);
			create_task_21(dataAp, nblocks, k, i, get_block);
		}

		for (i = k+1; i<nblocks; i++)
		{
			for (j = k+1; j<nblocks; j++)
			{
				create_task_22(dataAp, nblocks, k, i, j, get_block);
			}
		}
	}

	/* we wait the last task (TAG11(nblocks - 1)) and all the pivot tasks */
	starpu_tag_t *tags = malloc(nblocks*nblocks*sizeof(starpu_tag_t));
	unsigned ndeps = 0;

	tags[ndeps++] = TAG11(nblocks - 1);

	for (j = 0; j < nblocks; j++)
	{
		for (i = 0; i < j; i++)
		{
			tags[ndeps++] = PIVOT(j, i);
		}
	}

	/* schedule the codelet */
	gettimeofday(&start, NULL);
	int ret = starpu_submit_task(entry_task);
	if (STARPU_UNLIKELY(ret == -ENODEV))
	{
		fprintf(stderr, "No worker may execute this task\n");
		exit(-1);
	}

	/* stall the application until the end of computations */
	starpu_tag_wait_array(ndeps, tags);
//	starpu_wait_all_tasks();

	gettimeofday(&end, NULL);

	double timing = (double)((end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec));
	return timing;
}

starpu_data_handle get_block_with_striding(starpu_data_handle *dataAp,
			unsigned nblocks __attribute__((unused)), unsigned j, unsigned i)
{
	/* we use filters */
	return starpu_get_sub_data(*dataAp, 2, j, i);
}


void STARPU_LU(lu_decomposition_pivot)(TYPE *matA, unsigned *ipiv, unsigned size, unsigned ld, unsigned nblocks)
{
	starpu_data_handle dataA;

	/* monitor and partition the A matrix into blocks :
	 * one block is now determined by 2 unsigned (i,j) */
	starpu_register_blas_data(&dataA, 0, (uintptr_t)matA, ld, size, size, sizeof(TYPE));

	starpu_filter f;
		f.filter_func = starpu_vertical_block_filter_func;
		f.filter_arg = nblocks;

	starpu_filter f2;
		f2.filter_func = starpu_block_filter_func;
		f2.filter_arg = nblocks;

	starpu_map_filters(dataA, 2, &f, &f2);

	unsigned i;
	for (i = 0; i < size; i++)
		ipiv[i] = i;

	struct piv_s *piv_description = malloc(nblocks*sizeof(struct piv_s));
	unsigned block;
	for (block = 0; block < nblocks; block++)
	{
		piv_description[block].piv = ipiv;
		piv_description[block].first = block * (size / nblocks);
		piv_description[block].last = (block + 1) * (size / nblocks);
	}

#if 0
	unsigned j;
	for (j = 0; j < nblocks; j++)
	for (i = 0; i < nblocks; i++)
	{
		printf("BLOCK %d %d	%p\n", i, j, &matA[i*(size/nblocks) + j * (size/nblocks)*ld]);
	}
#endif

	double timing;
	timing = dw_codelet_facto_pivot(&dataA, piv_description, nblocks, get_block_with_striding);

	fprintf(stderr, "Computation took (in ms)\n");
	fprintf(stderr, "%2.2f\n", timing/1000);

	unsigned n = starpu_get_blas_nx(dataA);
	double flop = (2.0f*n*n*n)/3.0f;
	fprintf(stderr, "Synthetic GFlops : %2.2f\n", (flop/timing/1000.0f));

	/* gather all the data */
	starpu_unpartition_data(dataA, 0);
}


starpu_data_handle get_block_with_no_striding(starpu_data_handle *dataAp, unsigned nblocks, unsigned j, unsigned i)
{
	/* dataAp is an array of data handle */
	return dataAp[i+j*nblocks];
}

void STARPU_LU(lu_decomposition_pivot_no_stride)(TYPE **matA, unsigned *ipiv, unsigned size, unsigned ld, unsigned nblocks)
{
	starpu_data_handle *dataAp = malloc(nblocks*nblocks*sizeof(starpu_data_handle));

	/* monitor and partition the A matrix into blocks :
	 * one block is now determined by 2 unsigned (i,j) */
	unsigned bi, bj;
	for (bj = 0; bj < nblocks; bj++)
	for (bi = 0; bi < nblocks; bi++)
	{
		starpu_register_blas_data(&dataAp[bi+nblocks*bj], 0,
			(uintptr_t)matA[bi+nblocks*bj], size/nblocks,
			size/nblocks, size/nblocks, sizeof(TYPE));
	}

	unsigned i;
	for (i = 0; i < size; i++)
		ipiv[i] = i;

	struct piv_s *piv_description = malloc(nblocks*sizeof(struct piv_s));
	unsigned block;
	for (block = 0; block < nblocks; block++)
	{
		piv_description[block].piv = ipiv;
		piv_description[block].first = block * (size / nblocks);
		piv_description[block].last = (block + 1) * (size / nblocks);
	}

	double timing;
	timing = dw_codelet_facto_pivot(dataAp, piv_description, nblocks, get_block_with_no_striding);

	fprintf(stderr, "Computation took (in ms)\n");
	fprintf(stderr, "%2.2f\n", timing/1000);

	unsigned n = starpu_get_blas_nx(dataAp[0])*nblocks;
	double flop = (2.0f*n*n*n)/3.0f;
	fprintf(stderr, "Synthetic GFlops : %2.2f\n", (flop/timing/1000.0f));

	for (bj = 0; bj < nblocks; bj++)
	for (bi = 0; bi < nblocks; bi++)
	{
		starpu_delete_data(dataAp[bi+nblocks*bj]);
	}
}
