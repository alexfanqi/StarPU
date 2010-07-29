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

#include <starpu.h>
#include <pthread.h>

static unsigned niter = 16384;

/*
 *
 *		    /-->B--\
 *		    |      |
 *	     -----> A      D---\--->
 *		^   |      |   |
 *		|   \-->C--/   |
 *		|              |
 *		\--------------/
 *
 *	- {B, C} depend on A
 *	- D depends on {B, C}
 *	- A, B, C and D are resubmitted at the end of the loop (or not)
 */

static struct starpu_task taskA, taskB, taskC, taskD;

static unsigned loop_cnt = 0;
static unsigned check_cnt = 0;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void dummy_func(void *descr[] __attribute__ ((unused)), void *arg __attribute__ ((unused)))
{
	STARPU_ATOMIC_ADD(&check_cnt, 1);
}

static starpu_codelet dummy_codelet = 
{
	.where = STARPU_CPU|STARPU_CUDA|STARPU_OPENCL,
	.cpu_func = dummy_func,
	.cuda_func = dummy_func,
	.opencl_func = dummy_func,
	.model = NULL,
	.nbuffers = 0
};

static void callback_task_D(void *arg __attribute__((unused)))
{
	loop_cnt++;

	if (loop_cnt == niter)
	{
		/* We are done */
		pthread_mutex_lock(&mutex);
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);
	}
	else {
		/* Let's go for another iteration */
		starpu_task_submit(&taskA);
		starpu_task_submit(&taskB);
		starpu_task_submit(&taskC);
		starpu_task_submit(&taskD);
	}
}

int main(int argc, char **argv)
{
	unsigned i;
	double timing;
	struct timeval start;
	struct timeval end;

	starpu_init(NULL);

	starpu_task_init(&taskA);
	taskA.cl = &dummy_codelet;
	taskA.cl_arg = &taskA;

	starpu_task_init(&taskB);
	taskB.cl = &dummy_codelet;
	taskB.cl_arg = &taskB;

	starpu_task_init(&taskC);
	taskC.cl = &dummy_codelet;
	taskC.cl_arg = &taskC;

	starpu_task_init(&taskD);
	taskD.cl = &dummy_codelet;
	taskD.cl_arg = &taskD;
	taskD.callback_func = callback_task_D;

	struct starpu_task *depsBC_array[1] = {&taskA};
	starpu_task_declare_deps_array(&taskB, 1, depsBC_array);
	starpu_task_declare_deps_array(&taskC, 1, depsBC_array);

	struct starpu_task *depsD_array[2] = {&taskB, &taskC};
	starpu_task_declare_deps_array(&taskD, 2, depsD_array);

	starpu_task_submit(&taskA);
	starpu_task_submit(&taskB);
	starpu_task_submit(&taskC);
	starpu_task_submit(&taskD);

	/* Wait for the termination of all loops */
	pthread_mutex_lock(&mutex);
	if (loop_cnt < niter)
		pthread_cond_wait(&cond, &mutex);
	pthread_mutex_unlock(&mutex);

	STARPU_ASSERT(check_cnt == (4*loop_cnt));
	
	/* Cleanup the statically allocated tasks */
	starpu_task_deinit(&taskA);
	starpu_task_deinit(&taskB);
	starpu_task_deinit(&taskC);
	starpu_task_deinit(&taskD);


	starpu_shutdown();

	return 0;
}
