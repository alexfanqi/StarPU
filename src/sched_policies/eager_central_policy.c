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

#include <core/workers.h>
#include <sched_policies/fifo_queues.h>

/*
 *	This is just the trivial policy where every worker use the same
 *	JOB QUEUE.
 */

/* the former is the actual queue, the latter some container */
static struct starpu_fifo_taskq_s *fifo;

static pthread_cond_t sched_cond;
static pthread_mutex_t sched_mutex;

static void initialize_eager_center_policy(struct starpu_machine_topology_s *topology, 
		   __attribute__ ((unused)) struct starpu_sched_policy_s *_policy) 
{
	/* there is only a single queue in that trivial design */
	fifo = _starpu_create_fifo();

	PTHREAD_MUTEX_INIT(&sched_mutex, NULL);
	PTHREAD_COND_INIT(&sched_cond, NULL);

	unsigned workerid;
	for (workerid = 0; workerid < topology->nworkers; workerid++)
		starpu_worker_set_sched_condition(workerid, &sched_cond, &sched_mutex);
}

static void deinitialize_eager_center_policy(__attribute__ ((unused)) struct starpu_machine_topology_s *topology, 
		   __attribute__ ((unused)) struct starpu_sched_policy_s *_policy) 
{
	/* TODO check that there is no task left in the queue */

	/* deallocate the job queue */
	_starpu_destroy_fifo(fifo);
}

static int push_task_eager_policy(struct starpu_task *task)
{
	return _starpu_fifo_push_task(fifo, &sched_mutex, &sched_cond, task);
}

static int push_prio_task_eager_policy(struct starpu_task *task)
{
	return _starpu_fifo_push_prio_task(fifo, &sched_mutex, &sched_cond, task);
}

static struct starpu_task *pop_every_task_eager_policy(uint32_t where)
{
	return _starpu_fifo_pop_every_task(fifo, &sched_mutex, where);
}

static struct starpu_task *pop_task_eager_policy(void)
{
	return _starpu_fifo_pop_task(fifo);
}

struct starpu_sched_policy_s _starpu_sched_eager_policy = {
	.init_sched = initialize_eager_center_policy,
	.deinit_sched = deinitialize_eager_center_policy,
	.push_task = push_task_eager_policy,
	.push_prio_task = push_prio_task_eager_policy,
	.pop_task = pop_task_eager_policy,
	.pop_every_task = pop_every_task_eager_policy,
	.policy_name = "eager",
	.policy_description = "greedy policy"
};

struct starpu_sched_policy_s _starpu_sched_no_prio_policy = {
	.init_sched = initialize_eager_center_policy,
	.deinit_sched = deinitialize_eager_center_policy,
	.push_task = push_task_eager_policy,
	/* we use the same method in spite of the priority */
	.push_prio_task = push_task_eager_policy,
	.pop_task = pop_task_eager_policy,
	.pop_every_task = pop_every_task_eager_policy,
	.policy_name = "no-prio",
	.policy_description = "eager without priority"
};
