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

#ifndef __STARPU_SCHEDULER_H__
#define __STARPU_SCHEDULER_H__

#include <starpu.h>
#include <starpu_config.h>

#ifdef STARPU_HAVE_HWLOC
#include <hwloc.h>
#endif

struct starpu_task;

struct starpu_machine_topology_s {
	unsigned nworkers;

#ifdef STARPU_HAVE_HWLOC
	hwloc_topology_t hwtopology;
#else
	/* We maintain ABI compatibility with and without hwloc */
	void *dummy;
#endif

	unsigned nhwcpus;
        unsigned nhwcudagpus;
        unsigned nhwopenclgpus;

	unsigned ncpus;
	unsigned ncudagpus;
	unsigned nopenclgpus;
	unsigned ngordon_spus;

	/* Where to bind workers ? */
	unsigned workers_bindid[STARPU_NMAXWORKERS];
	
	/* Which GPU(s) do we use for CUDA ? */
	unsigned workers_cuda_gpuid[STARPU_NMAXWORKERS];

	/* Which GPU(s) do we use for OpenCL ? */
	unsigned workers_opencl_gpuid[STARPU_NMAXWORKERS];
};

/* This structure contains all the methods that implement a scheduling policy.
 * An application may specify which scheduling strategy in the "sched_policy"
 * field of the starpu_conf structure passed to the starpu_init function. */
struct starpu_sched_policy_s {
	/* Initialize the scheduling policy. */
	void (*init_sched)(struct starpu_machine_topology_s *, struct starpu_sched_policy_s *);

	/* Cleanup the scheduling policy. */
	void (*deinit_sched)(struct starpu_machine_topology_s *, struct starpu_sched_policy_s *);

	/* Insert a task into the scheduler. */
	int (*push_task)(struct starpu_task *);

	/* Insert a priority task into the scheduler. */
	int (*push_prio_task)(struct starpu_task *);

	/* Get a task from the scheduler. The mutex associated to the worker is
	 * already taken when this method is called. */
	struct starpu_task *(*pop_task)(void);

	 /* Remove all available tasks from the scheduler (tasks are chained by
	  * the means of the prev and next fields of the starpu_task
	  * structure). The mutex associated to the worker is already taken
	  * when this method is called. */
	struct starpu_task *(*pop_every_task)(uint32_t where);

	/* This method is called every time a task has been executed. (optionnal) */
	void (*post_exec_hook)(struct starpu_task *);

	/* Name of the policy (optionnal) */
	const char *policy_name;

	/* Description of the policy (optionnal) */
	const char *policy_description;
};

/* When there is no available task for a worker, StarPU blocks this worker on a
condition variable. This function specifies which condition variable (and the
associated mutex) should be used to block (and to wake up) a worker. Note that
multiple workers may use the same condition variable. For instance, in the case
of a scheduling strategy with a single task queue, the same condition variable
would be used to block and wake up all workers.  The initialization method of a
scheduling strategy (init_sched) must call this function once per worker. */
void starpu_worker_set_sched_condition(int workerid, pthread_cond_t *sched_cond, pthread_mutex_t *sched_mutex);

#endif // __STARPU_SCHEDULER_H__
