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
#include <starpu.h>

#define NTASKS	32000

struct starpu_task_list sched_list;

static pthread_cond_t sched_cond;
static pthread_mutex_t sched_mutex;

static void init_dummy_sched(struct starpu_machine_topology_s *topology,
			struct starpu_sched_policy_s *policy)
{
	/* Create a linked-list of tasks and a condition variable to protect it */
	starpu_task_list_init(&sched_list);

	pthread_mutex_init(&sched_mutex, NULL);
	pthread_cond_init(&sched_cond, NULL);

	unsigned workerid;
	for (workerid = 0; workerid < topology->nworkers; workerid++)
		starpu_worker_set_sched_condition(workerid, &sched_cond, &sched_mutex);
}

static void deinit_dummy_sched(struct starpu_machine_topology_s *topology,
				struct starpu_sched_policy_s *policy)
{
	STARPU_ASSERT(starpu_task_list_empty(&sched_list));

	pthread_cond_destroy(&sched_cond);
	pthread_mutex_destroy(&sched_mutex);
}

static int push_task_dummy(struct starpu_task *task)
{
	pthread_mutex_lock(&sched_mutex);

	starpu_task_list_push_front(&sched_list, task);

	pthread_cond_signal(&sched_cond);

	pthread_mutex_unlock(&sched_mutex);
}

/* The mutex associated to the calling worker is already taken by StarPU */
static struct starpu_task *pop_task_dummy(void)
{
	/* NB: In this simplistic strategy, we assume that all workers are able
	 * to execute all tasks, otherwise, it would have been necessary to go
	 * through the entire list until we find a task that is executable from
	 * the calling worker. So we just take the head of the list and give it
	 * to the worker. */
	return starpu_task_list_pop_back(&sched_list);
}

static struct starpu_sched_policy_s dummy_sched_policy = {
	.init_sched = init_dummy_sched,
	.deinit_sched = deinit_dummy_sched,
	.push_task = push_task_dummy,
	.pop_task = pop_task_dummy,
	.pop_every_task = NULL,
	.policy_name = "dummy",
	.policy_description = "dummy scheduling strategy"
};

static struct starpu_conf conf = {
	.sched_policy_name = NULL,
	.sched_policy = &dummy_sched_policy,
	.ncpus = -1,
	.ncuda = -1,
        .nopencl = -1,
	.nspus = -1,
	.use_explicit_workers_bindid = 0,
	.use_explicit_workers_cuda_gpuid = 0,
	.use_explicit_workers_opencl_gpuid = 0,
	.calibrate = 0
};

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
	starpu_init(&conf);

	unsigned i;
	for (i = 0; i < NTASKS; i++)
	{
		struct starpu_task *task = starpu_task_create();
	
		task->cl = &dummy_codelet;
		task->cl_arg = NULL;
	
		starpu_task_submit(task);
	}

	starpu_task_wait_for_all();

	starpu_shutdown();

	return 0;
}
