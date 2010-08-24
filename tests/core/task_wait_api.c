/*
 * StarPU
 * Copyright (C) Université Bordeaux 1, CNRS 2008-2010 (see AUTHORS file)
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

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include <starpu.h>

static void dummy_func(void *descr[] __attribute__ ((unused)), void *arg __attribute__ ((unused)))
{
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

static struct starpu_task *create_dummy_task(void)
{
	struct starpu_task *task = starpu_task_create();

	task->cl = &dummy_codelet;
	task->cl_arg = NULL;

	return task;
}

int main(int argc, char **argv)
{
	starpu_init(NULL);

	fprintf(stderr, "{ A } -> { B }\n");
	fflush(stderr);

	struct starpu_task *taskA, *taskB;
	
	taskA = create_dummy_task();
	taskB = create_dummy_task();

	/* B depends on A */
	starpu_task_declare_deps_array(taskB, 1, &taskA);

	starpu_task_submit(taskB);
	starpu_task_submit(taskA);

	starpu_task_wait(taskB);

	fprintf(stderr, "{ C, D, E, F } -> { G }\n");

	struct starpu_task *taskC, *taskD, *taskE, *taskF, *taskG;

	taskC = create_dummy_task();
	taskD = create_dummy_task();
	taskE = create_dummy_task();
	taskF = create_dummy_task();
	taskG = create_dummy_task();

	struct starpu_task *tasksCDEF[4] = {taskC, taskD, taskE, taskF};
	starpu_task_declare_deps_array(taskG, 4, tasksCDEF);

	starpu_task_submit(taskC);
	starpu_task_submit(taskD);
	starpu_task_submit(taskG);
	starpu_task_submit(taskE);
	starpu_task_submit(taskF);

	starpu_task_wait(taskG);

	fprintf(stderr, "{ H, I } -> { J, K, L }\n");
	
	struct starpu_task *taskH, *taskI, *taskJ, *taskK, *taskL;

	taskH = create_dummy_task();
	taskI = create_dummy_task();
	taskJ = create_dummy_task();
	taskK = create_dummy_task();
	taskL = create_dummy_task();

	struct starpu_task *tasksHI[2] = {taskH, taskI};

	starpu_task_declare_deps_array(taskJ, 2, tasksHI);
	starpu_task_declare_deps_array(taskK, 2, tasksHI);
	starpu_task_declare_deps_array(taskL, 2, tasksHI);

	starpu_task_submit(taskH);
	starpu_task_submit(taskI);
	starpu_task_submit(taskJ);
	starpu_task_submit(taskK);
	starpu_task_submit(taskL);

	starpu_task_wait(taskJ);
	starpu_task_wait(taskK);
	starpu_task_wait(taskL);

	starpu_shutdown();

	return 0;
}
