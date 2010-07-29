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
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <starpu.h>

static unsigned ntasks = 1024;

static void callback(void *arg)
{
	struct starpu_task *task = starpu_get_current_task();

	unsigned *cnt = arg;

	(*cnt)++;

	if (*cnt == ntasks)
		task->regenerate = 0;
}

static void dummy_func(void *descr[] __attribute__ ((unused)), void *arg __attribute__ ((unused)))
{
}

static starpu_codelet dummy_codelet = 
{
	.where = STARPU_CPU|STARPU_CUDA,
	.cpu_func = dummy_func,
	.cuda_func = dummy_func,
	.model = NULL,
	.nbuffers = 0
};

static void parse_args(int argc, char **argv)
{
	int c;
	while ((c = getopt(argc, argv, "i:")) != -1)
	switch(c) {
		case 'i':
			ntasks = atoi(optarg);
			break;
	}
}

#define K	128

int main(int argc, char **argv)
{
	double timing;
	struct timeval start;
	struct timeval end;

	parse_args(argc, argv);

	starpu_init(NULL);

	struct starpu_task task[K];
	unsigned cnt[K];;

	int i;
	for (i = 0; i < K; i++)
	{
		starpu_task_init(&task[i]);
		cnt[i] = 0;

		task[i].cl = &dummy_codelet;
		task[i].regenerate = 1;
		task[i].detach = 1;

		task[i].callback_func = callback;
		task[i].callback_arg = &cnt[i];
	}

	fprintf(stderr, "#tasks : %d x %d tasks\n", K, ntasks);

	gettimeofday(&start, NULL);
	
	for (i = 0; i < K; i++)
		starpu_task_submit(&task[i]);

	starpu_task_wait_for_all();

	gettimeofday(&end, NULL);

	/* Check that all the tasks have been properly executed */
	unsigned total_cnt = 0;
	for (i = 0; i < K; i++)
		total_cnt += cnt[i];

	STARPU_ASSERT(total_cnt == K*ntasks);

	timing = (double)((end.tv_sec - start.tv_sec)*1000000
				+ (end.tv_usec - start.tv_usec));

	fprintf(stderr, "Total: %lf secs\n", timing/1000000);
	fprintf(stderr, "Per task: %lf usecs\n", timing/(K*ntasks));

	starpu_shutdown();

	return 0;
}
