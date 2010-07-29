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

#include <sys/time.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include <starpu.h>

static starpu_tag_t tagA = 0x0042;
static starpu_tag_t tagB = 0x1042;
static starpu_tag_t tagC = 0x2042;
static starpu_tag_t tagD = 0x3042;
static starpu_tag_t tagE = 0x4042;
static starpu_tag_t tagF = 0x5042;

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

int main(int argc, char **argv)
{
	starpu_init(NULL);

	/* {A,B,C} -> D -> {E,F}, D is empty */
	struct starpu_task *taskA = starpu_task_create();
	taskA->cl = &dummy_codelet;
	taskA->use_tag = 1;
	taskA->tag_id = tagA;
	
	struct starpu_task *taskB = starpu_task_create();
	taskB->cl = &dummy_codelet;
	taskB->use_tag = 1;
	taskB->tag_id = tagB;
	
	struct starpu_task *taskC = starpu_task_create();
	taskC->cl = &dummy_codelet;
	taskC->use_tag = 1;
	taskC->tag_id = tagC;

	struct starpu_task *taskD = starpu_task_create();
	taskD->cl = NULL;
	taskD->use_tag = 1;
	taskD->tag_id = tagD;
	starpu_tag_declare_deps(tagD, 3, tagA, tagB, tagC);

	struct starpu_task *taskE = starpu_task_create();
	taskE->cl = &dummy_codelet;
	taskE->use_tag = 1;
	taskE->tag_id = tagE;
	starpu_tag_declare_deps(tagE, 1, tagD);

	struct starpu_task *taskF = starpu_task_create();
	taskF->cl = &dummy_codelet;
	taskF->use_tag = 1;
	taskF->tag_id = tagF;
	starpu_tag_declare_deps(tagF, 1, tagD);

	starpu_task_submit(taskA);
	starpu_task_submit(taskB);
	starpu_task_submit(taskC);
	starpu_task_submit(taskD);
	starpu_task_submit(taskE);
	starpu_task_submit(taskF);

	starpu_tag_t tag_array[2] = {tagE, tagF};
	starpu_tag_wait_array(2, tag_array);

	starpu_shutdown();

	return 0;
}
