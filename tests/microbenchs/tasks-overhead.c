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

#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include <starpu.h>

starpu_data_handle data_handles[8];
float *buffers[8];

static unsigned ntasks = 65536;
static unsigned nbuffers = 0;

struct starpu_task *tasks;

static void dummy_func(void *descr[] __attribute__ ((unused)), void *arg __attribute__ ((unused)))
{
}

static starpu_codelet dummy_codelet = 
{
	.where = STARPU_CORE|STARPU_CUDA,
	.core_func = dummy_func,
	.cuda_func = dummy_func,
	.model = NULL,
	.nbuffers = 0
};

void inject_one_task(void)
{
	struct starpu_task *task = starpu_task_create();

	task->cl = &dummy_codelet;
	task->cl_arg = NULL;
	task->callback_func = NULL;
	task->synchronous = 1;

	starpu_submit_task(task);
}

static void parse_args(int argc, char **argv)
{
	int c;
	while ((c = getopt(argc, argv, "i:b:h")) != -1)
	switch(c) {
		case 'i':
			ntasks = atoi(optarg);
			break;
		case 'b':
			nbuffers = atoi(optarg);
			dummy_codelet.nbuffers = nbuffers;
			break;
		case 'h':
			fprintf(stderr, "Usage: %s [-i ntasks] [-b nbuffers] [-h]\n", argv[0]);
			break;
	}
}

int main(int argc, char **argv)
{
	unsigned i;

	double timing_submit;
	struct timeval start_submit;
	struct timeval end_submit;

	double timing_exec;
	struct timeval start_exec;
	struct timeval end_exec;

	parse_args(argc, argv);

	unsigned buffer;
	for (buffer = 0; buffer < nbuffers; buffer++)
	{
		buffers[buffer] = malloc(16*sizeof(float));
		starpu_register_vector_data(&data_handles[buffer], 0, (uintptr_t)buffers[buffer], 16, sizeof(float));
	}

	starpu_init(NULL);

	fprintf(stderr, "#tasks : %d\n#buffers : %d\n", ntasks, nbuffers);

	/* submit tasks (but don't execute them yet !) */
	tasks = malloc(ntasks*sizeof(struct starpu_task));

	gettimeofday(&start_submit, NULL);
	for (i = 0; i < ntasks; i++)
	{
		tasks[i].callback_func = NULL;
		tasks[i].cl = &dummy_codelet;
		tasks[i].cl_arg = NULL;
		tasks[i].synchronous = 0;
		tasks[i].use_tag = 1;
		tasks[i].tag_id = (starpu_tag_t)i;

		/* we have 8 buffers at most */
		for (buffer = 0; buffer < nbuffers; buffer++)
		{
			tasks[i].buffers[buffer].handle = data_handles[buffer];
			tasks[i].buffers[buffer].mode = STARPU_RW;
		}
	}

	gettimeofday(&start_submit, NULL);
	for (i = 1; i < ntasks; i++)
	{
		starpu_tag_declare_deps((starpu_tag_t)i, 1, (starpu_tag_t)(i-1));

		starpu_submit_task(&tasks[i]);
	}

	/* submit the first task */
	starpu_submit_task(&tasks[0]);

	gettimeofday(&end_submit, NULL);

	/* wait for the execution of the tasks */
	gettimeofday(&start_exec, NULL);
	starpu_tag_wait((starpu_tag_t)(ntasks - 1));
	gettimeofday(&end_exec, NULL);

	timing_submit = (double)((end_submit.tv_sec - start_submit.tv_sec)*1000000 + (end_submit.tv_usec - start_submit.tv_usec));
	timing_exec = (double)((end_exec.tv_sec - start_exec.tv_sec)*1000000 + (end_exec.tv_usec - start_exec.tv_usec));

	fprintf(stderr, "Total submit: %lf secs\n", timing_submit/1000000);
	fprintf(stderr, "Per task submit: %lf usecs\n", timing_submit/ntasks);
	fprintf(stderr, "\n");
	fprintf(stderr, "Total execution: %lf secs\n", timing_exec/1000000);
	fprintf(stderr, "Per task execution: %lf usecs\n", timing_exec/ntasks);
	fprintf(stderr, "\n");
	fprintf(stderr, "Total: %lf secs\n", (timing_submit+timing_exec)/1000000);
	fprintf(stderr, "Per task: %lf usecs\n", (timing_submit+timing_exec)/ntasks);

	starpu_shutdown();

	return 0;
}
