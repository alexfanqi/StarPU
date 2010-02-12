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

#include <starpu.h>

/* number of philosophers */
#define N	16

starpu_data_handle fork_handles[N];
unsigned forks[N];

static void eat_kernel(void *descr[], void *arg)
{
}

static starpu_codelet eating_cl = {
	.where = STARPU_CORE|STARPU_CUDA,
	.cuda_func = eat_kernel,
	.core_func = eat_kernel,
	.nbuffers = 2
};

void submit_one_task(unsigned p)
{
	struct starpu_task *task = starpu_task_create();

	task->cl = &eating_cl;

	unsigned left = p;
	unsigned right = (p+1)%N;

	/* TODO we should not have to order these ressources ! */
	/* the last philosopher is left-handed ;) */
	if (p == (N - 1))
	{
		unsigned tmp;
		tmp = right;
		right = left;
		left = tmp;
	}

	task->buffers[0].handle = fork_handles[left];
	task->buffers[0].mode = STARPU_RW;
	task->buffers[1].handle = fork_handles[right];
	task->buffers[1].mode = STARPU_RW;

	int ret = starpu_submit_task(task);
	STARPU_ASSERT(!ret);
}

int main(int argc, int argv)
{
	starpu_init(NULL);

	/* initialize the forks */
	unsigned f;
	for (f = 0; f < N; f++)
	{
		forks[f] = 0;

		starpu_register_vector_data(&fork_handles[f], 0, (uintptr_t)&forks[f], 1, sizeof(unsigned));
	}

	unsigned ntasks = 1024;

	unsigned t;
	for (t = 0; t < ntasks; t++)
	{
		/* select one philosopher randomly */
		unsigned philosopher = rand() % N;
		submit_one_task(philosopher);
	}

	starpu_wait_all_tasks();

	starpu_shutdown();

	return 0;
}
