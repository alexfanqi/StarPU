/*
 * StarPU
 * Copyright (C) Université Bordeaux 1, CNRS 2008-2009 (see AUTHORS file)
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
#include <unistd.h>
#include <errno.h>
#include <starpu.h>
#include <stdlib.h>
#include <pthread.h>

#define N	10000

#define VECTORSIZE	1024

static pthread_mutex_t mutex;
static pthread_cond_t cond;

static unsigned finished = 0;

static unsigned cnt = N;

starpu_data_handle v_handle, v_handle2;
static unsigned *v;
static unsigned *v2;

static void callback(void *arg)
{
	unsigned res = STARPU_ATOMIC_ADD(&cnt, -1);

	if (res == 0)
	{
		pthread_mutex_lock(&mutex);
		finished = 1;
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);
	}
}



static void cuda_codelet_null(void *descr[], __attribute__ ((unused)) void *_args)
{
}

static void opencl_codelet_null(void *descr[], __attribute__ ((unused)) void *_args)
{
}

static void cpu_codelet_null(void *descr[], __attribute__ ((unused)) void *_args)
{
}

static starpu_access_mode select_random_mode(void)
{
	int r = rand();

	switch (r % 3) {
		case 0:
			return STARPU_R;
		case 1:
			return STARPU_W;
		case 2:
			return STARPU_RW;
	};
}


static starpu_codelet cl = {
	.where = STARPU_CPU|STARPU_CUDA|STARPU_OPENCL,
	.cpu_func = cpu_codelet_null,
	.cuda_func = cuda_codelet_null,
        .opencl_func = opencl_codelet_null,
	.nbuffers = 2
};


int main(int argc, char **argv)
{
	starpu_init(NULL);

	starpu_data_malloc_pinned_if_possible((void **)&v, VECTORSIZE*sizeof(unsigned));
	starpu_data_malloc_pinned_if_possible((void **)&v2, VECTORSIZE*sizeof(unsigned));

	starpu_vector_data_register(&v_handle, 0, (uintptr_t)v, VECTORSIZE, sizeof(unsigned));
	starpu_vector_data_register(&v_handle2, 0, (uintptr_t)v2, VECTORSIZE, sizeof(unsigned));

	unsigned iter;
	for (iter = 0; iter < N; iter++)
	{
		struct starpu_task *task = starpu_task_create();
		task->cl = &cl;

		task->buffers[0].handle = v_handle;
		task->buffers[0].mode = select_random_mode();

		task->buffers[1].handle = v_handle2;
		task->buffers[1].mode = select_random_mode();

		task->callback_func = callback;
		task->callback_arg = NULL;

		int ret = starpu_task_submit(task);
		if (ret == -ENODEV)
			goto enodev;
	}

	pthread_mutex_lock(&mutex);
	if (!finished)
		pthread_cond_wait(&cond, &mutex);
	pthread_mutex_unlock(&mutex);

	starpu_shutdown();

	return 0;

enodev:
	fprintf(stderr, "WARNING: No one can execute this task\n");
	/* yes, we do not perform the computation but we did detect that no one
 	 * could perform the kernel, so this is not an error from StarPU */
	return 0;
}
