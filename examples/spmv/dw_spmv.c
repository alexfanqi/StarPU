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

/*
 * Conjugate gradients for Sparse matrices
 */

#include "dw_spmv.h"

struct timeval start;
struct timeval end;

unsigned nblocks = 1;
unsigned remainingtasks = -1;

#ifdef STARPU_USE_CUDA

extern void spmv_kernel_cpu_wrapper(uint32_t nnz, uint32_t nrow, float *nzval,
			uint32_t *colind, uint32_t *rowptr, uint32_t firstentry,
			float *vecin, uint32_t nx_in,
			float * vecout, uint32_t nx_out);

void spmv_kernel_cuda(void *descr[], void *args)
{
	uint32_t nnz = STARPU_GET_CSR_NNZ(descr[0]);
	uint32_t nrow = STARPU_GET_CSR_NROW(descr[0]);
	float *nzval = (float *)STARPU_GET_CSR_NZVAL(descr[0]);
	uint32_t *colind = STARPU_GET_CSR_COLIND(descr[0]);
	uint32_t *rowptr = STARPU_GET_CSR_ROWPTR(descr[0]);
	uint32_t firstentry = STARPU_GET_CSR_FIRSTENTRY(descr[0]);

	float *vecin = (float *)STARPU_GET_VECTOR_PTR(descr[1]);
	uint32_t nx_in = STARPU_GET_VECTOR_NX(descr[1]);

	float *vecout = (float *)STARPU_GET_VECTOR_PTR(descr[2]);
	uint32_t nx_out = STARPU_GET_VECTOR_NX(descr[2]);

	spmv_kernel_cpu_wrapper(nnz, nrow, nzval, colind, rowptr, firstentry, vecin, nx_in, vecout, nx_out);
}

#endif // STARPU_USE_CUDA


sem_t sem;
uint32_t size = 4194304;

starpu_data_handle sparse_matrix;
starpu_data_handle vector_in, vector_out;

float *sparse_matrix_nzval;
uint32_t *sparse_matrix_colind;
uint32_t *sparse_matrix_rowptr;

float *vector_in_ptr;
float *vector_out_ptr;

unsigned usecpu = 0;


void parse_args(int argc, char **argv)
{
	int i;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-size") == 0) {
			char *argptr;
			size = strtol(argv[++i], &argptr, 10);
		}

		if (strcmp(argv[i], "-nblocks") == 0) {
			char *argptr;
			nblocks = strtol(argv[++i], &argptr, 10);
		}


		if (strcmp(argv[i], "-cpu") == 0) {
			usecpu = 1;
		}
	}
}

void cpu_spmv(void *descr[], __attribute__((unused))  void *arg)
{
	float *nzval = (float *)STARPU_GET_CSR_NZVAL(descr[0]);
	uint32_t *colind = STARPU_GET_CSR_COLIND(descr[0]);
	uint32_t *rowptr = STARPU_GET_CSR_ROWPTR(descr[0]);

	float *vecin = (float *)STARPU_GET_VECTOR_PTR(descr[1]);
	float *vecout = (float *)STARPU_GET_VECTOR_PTR(descr[2]);

	uint32_t firstelem = STARPU_GET_CSR_FIRSTENTRY(descr[0]);

	uint32_t nnz;
	uint32_t nrow;

	nnz = STARPU_GET_CSR_NNZ(descr[0]);
	nrow = STARPU_GET_CSR_NROW(descr[0]);

	//STARPU_ASSERT(nrow == STARPU_GET_VECTOR_NX(descr[1]));
	STARPU_ASSERT(nrow == STARPU_GET_VECTOR_NX(descr[2]));

	unsigned row;
	for (row = 0; row < nrow; row++)
	{
		float tmp = 0.0f;
		unsigned index;

		unsigned firstindex = rowptr[row] - firstelem;
		unsigned lastindex = rowptr[row+1] - firstelem;

		for (index = firstindex; index < lastindex; index++)
		{
			unsigned col;

			col = colind[index];
			tmp += nzval[index]*vecin[col];
		}

		vecout[row] = tmp;
	}

}

void create_data(void)
{
	/* we need a sparse symetric (definite positive ?) matrix and a "dense" vector */
	
	/* example of 3-band matrix */
	float *nzval;
	uint32_t nnz;
	uint32_t *colind;
	uint32_t *rowptr;

	nnz = 3*size-2;

	nzval = malloc(nnz*sizeof(float));
	colind = malloc(nnz*sizeof(uint32_t));
	rowptr = malloc((size+1)*sizeof(uint32_t));

	assert(nzval);
	assert(colind);
	assert(rowptr);

	/* fill the matrix */
	unsigned row;
	unsigned pos = 0;
	for (row = 0; row < size; row++)
	{
		rowptr[row] = pos;

		if (row > 0) {
			nzval[pos] = 1.0f;
			colind[pos] = row-1;
			pos++;
		}
		
		nzval[pos] = 5.0f;
		colind[pos] = row;
		pos++;

		if (row < size - 1) {
			nzval[pos] = 1.0f;
			colind[pos] = row+1;
			pos++;
		}
	}

	STARPU_ASSERT(pos == nnz);

	rowptr[size] = nnz;
	
	starpu_register_csr_data(&sparse_matrix, 0, nnz, size, (uintptr_t)nzval, colind, rowptr, 0, sizeof(float));

	sparse_matrix_nzval = nzval;
	sparse_matrix_colind = colind;
	sparse_matrix_rowptr = rowptr;

	/* initiate the 2 vectors */
	float *invec, *outvec;
	invec = malloc(size*sizeof(float));
	assert(invec);

	outvec = malloc(size*sizeof(float));
	assert(outvec);

	/* fill those */
	unsigned ind;
	for (ind = 0; ind < size; ind++)
	{
		invec[ind] = 2.0f;
		outvec[ind] = 0.0f;
	}

	starpu_register_vector_data(&vector_in, 0, (uintptr_t)invec, size, sizeof(float));
	starpu_register_vector_data(&vector_out, 0, (uintptr_t)outvec, size, sizeof(float));

	vector_in_ptr = invec;
	vector_out_ptr = outvec;

}

void init_problem_callback(void *arg)
{
	unsigned *remaining = arg;


	unsigned val = STARPU_ATOMIC_ADD(remaining, -1);

	printf("callback %d remaining \n", val);
	if ( val == 0 )
	{
		printf("DONE ...\n");
		gettimeofday(&end, NULL);

		starpu_unpartition_data(sparse_matrix, 0);
		starpu_unpartition_data(vector_out, 0);

		sem_post(&sem);
	}
}


void call_spmv_codelet_filters(void)
{

	remainingtasks = nblocks;

	starpu_codelet *cl = malloc(sizeof(starpu_codelet));

	/* partition the data along a block distribution */
	starpu_filter csr_f, vector_f;
	csr_f.filter_func    = starpu_vertical_block_filter_func_csr;
	csr_f.filter_arg     = nblocks;
	vector_f.filter_func = starpu_block_filter_func_vector;
	vector_f.filter_arg  = nblocks;

	starpu_partition_data(sparse_matrix, &csr_f);
	starpu_partition_data(vector_out, &vector_f);

	cl->where = STARPU_CPU|STARPU_CUDA;
	cl->cpu_func =  cpu_spmv;
#ifdef STARPU_USE_CUDA
	cl->cuda_func = spmv_kernel_cuda;
#endif
	cl->nbuffers = 3;

	gettimeofday(&start, NULL);

	unsigned part;
	for (part = 0; part < nblocks; part++)
	{
		struct starpu_task *task = starpu_task_create();

		task->callback_func = init_problem_callback;
		task->callback_arg = &remainingtasks;
		task->cl = cl;
		task->cl_arg = NULL;
	
		task->buffers[0].handle = starpu_get_sub_data(sparse_matrix, 1, part);
		task->buffers[0].mode  = STARPU_R;
		task->buffers[1].handle = vector_in;
		task->buffers[1].mode = STARPU_R;
		task->buffers[2].handle = starpu_get_sub_data(vector_out, 1, part);
		task->buffers[2].mode = STARPU_W;
	
		starpu_submit_task(task);
	}
}

void init_problem(void)
{
	/* create the sparse input matrix */
	create_data();

	/* create a new codelet that will perform a SpMV on it */
	call_spmv_codelet_filters();
}

void print_results(void)
{
	unsigned row;

	for (row = 0; row < STARPU_MIN(size, 16); row++)
	{
		printf("%2.2f\t%2.2f\n", vector_in_ptr[row], vector_out_ptr[row]);
	}
}

int main(__attribute__ ((unused)) int argc,
	__attribute__ ((unused)) char **argv)
{
	parse_args(argc, argv);

	/* start the runtime */
	starpu_init(NULL);

	sem_init(&sem, 0, 0U);

	init_problem();

	sem_wait(&sem);
	sem_destroy(&sem);

	print_results();

	double timing = (double)((end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec));
	fprintf(stderr, "Computation took (in ms)\n");
	printf("%2.2f\n", timing/1000);

	return 0;
}
