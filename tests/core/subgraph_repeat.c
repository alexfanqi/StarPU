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

static unsigned niter = 128;

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
 *	- A is resubmitted at the end of the loop (or not)
 */

static struct starpu_task taskA, taskB, taskC, taskD;

static unsigned loop_cnt = 0;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void dummy_func(void *descr[] __attribute__ ((unused)), void *arg __attribute__ ((unused)))
{
	fprintf(stderr, "PIF\n");
}

static starpu_codelet dummy_codelet = 
{
	.where = STARPU_CPU|STARPU_CUDA,
	.cpu_func = dummy_func,
	.cuda_func = dummy_func,
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
		starpu_submit_task(&taskA);
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
	taskA.regenerate = 0; /* this must be submitted again */

	starpu_task_init(&taskB);
	taskB.cl = &dummy_codelet;
	taskB.regenerate = 1;

	starpu_task_init(&taskC);
	taskC.cl = &dummy_codelet;
	taskC.regenerate = 1;

	starpu_task_init(&taskD);
	taskD.cl = &dummy_codelet;
	taskD.regenerate = 1;
	taskD.callback_func = callback_task_D;

	struct starpu_task *depsBC_array[1] = {&taskA};
	starpu_task_declare_deps_array(&taskB, 1, depsBC_array);
	starpu_task_declare_deps_array(&taskC, 1, depsBC_array);

	struct starpu_task *depsD_array[2] = {&taskB, &taskC};
	starpu_task_declare_deps_array(&taskD, 2, depsD_array);

	starpu_submit_task(&taskA);
	starpu_submit_task(&taskB);
	starpu_submit_task(&taskC);
	starpu_submit_task(&taskD);

	/* Wait for the termination of all loops */
	pthread_mutex_lock(&mutex);
	if (loop_cnt < niter)
		pthread_cond_wait(&cond, &mutex);
	pthread_mutex_unlock(&mutex);

	starpu_shutdown();

	return 0;
}