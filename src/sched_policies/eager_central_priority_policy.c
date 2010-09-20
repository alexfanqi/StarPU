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

/*
 *	This is policy where every worker use the same JOB QUEUE, but taking
 *	task priorities into account
 */

#include <starpu.h>
#include <starpu_scheduler.h>
#include <common/config.h>
#include <core/workers.h>
#include <common/utils.h>

#define MIN_LEVEL	(-5)
#define MAX_LEVEL	(+5)

#define NPRIO_LEVELS	(MAX_LEVEL - MIN_LEVEL + 1)

struct starpu_priority_taskq_s {
	/* the actual lists 
	 *	taskq[p] is for priority [p - STARPU_MIN_PRIO] */
	struct starpu_task_list taskq[NPRIO_LEVELS];
	unsigned ntasks[NPRIO_LEVELS];

	unsigned total_ntasks;
};

/* the former is the actual queue, the latter some container */
static struct starpu_priority_taskq_s *taskq;

/* keep track of the total number of tasks to be scheduled to avoid infinite 
 * polling when there are really few tasks in the overall queue */
static pthread_cond_t global_sched_cond;
static pthread_mutex_t global_sched_mutex;

/*
 * Centralized queue with priorities 
 */

static struct starpu_priority_taskq_s *_starpu_create_priority_taskq(void)
{
	struct starpu_priority_taskq_s *central_queue;
	
	central_queue = malloc(sizeof(struct starpu_priority_taskq_s));
	central_queue->total_ntasks = 0;

	unsigned prio;
	for (prio = 0; prio < NPRIO_LEVELS; prio++)
	{
		starpu_task_list_init(&central_queue->taskq[prio]);
		central_queue->ntasks[prio] = 0;
	}

	return central_queue;
}

static void _starpu_destroy_priority_taskq(struct starpu_priority_taskq_s *priority_queue)
{
	free(priority_queue);
}

static void initialize_eager_center_priority_policy(struct starpu_machine_topology_s *topology, 
			__attribute__ ((unused))	struct starpu_sched_policy_s *_policy) 
{
	/* In this policy, we support more than two levels of priority. */
	starpu_sched_set_min_priority(MIN_LEVEL);
	starpu_sched_set_max_priority(MAX_LEVEL);

	/* only a single queue (even though there are several internaly) */
	taskq = _starpu_create_priority_taskq();

	PTHREAD_MUTEX_INIT(&global_sched_mutex, NULL);
	PTHREAD_COND_INIT(&global_sched_cond, NULL);

	unsigned workerid;
	for (workerid = 0; workerid < topology->nworkers; workerid++)
		starpu_worker_set_sched_condition(workerid, &global_sched_cond, &global_sched_mutex);
}

static void deinitialize_eager_center_priority_policy(struct starpu_machine_topology_s *topology,
		   __attribute__ ((unused)) struct starpu_sched_policy_s *_policy) 
{
	/* TODO check that there is no task left in the queue */

	/* deallocate the task queue */
	_starpu_destroy_priority_taskq(taskq);
}

static int _starpu_priority_push_task(struct starpu_task *task)
{
	/* wake people waiting for a task */
	PTHREAD_MUTEX_LOCK(&global_sched_mutex);

	STARPU_TRACE_JOB_PUSH(task, 1);
	
	unsigned priolevel = task->priority - STARPU_MIN_PRIO;

	starpu_task_list_push_front(&taskq->taskq[priolevel], task);
	taskq->ntasks[priolevel]++;
	taskq->total_ntasks++;

	PTHREAD_COND_SIGNAL(&global_sched_cond);
	PTHREAD_MUTEX_UNLOCK(&global_sched_mutex);

	return 0;
}

static struct starpu_task *_starpu_priority_pop_task(void)
{
	struct starpu_task *task = NULL;

	/* block until some event happens */
	PTHREAD_MUTEX_LOCK(&global_sched_mutex);

	if ((taskq->total_ntasks == 0) && _starpu_machine_is_running())
	{
#ifdef STARPU_NON_BLOCKING_DRIVERS
		PTHREAD_MUTEX_UNLOCK(&global_sched_mutex);
		return NULL;
#else
		PTHREAD_COND_WAIT(&global_sched_cond, &global_sched_mutex);
#endif
	}

	if (taskq->total_ntasks > 0)
	{
		unsigned priolevel = NPRIO_LEVELS - 1;
		do {
			if (taskq->ntasks[priolevel] > 0) {
				/* there is some task that we can grab */
				task = starpu_task_list_pop_back(&taskq->taskq[priolevel]);
				taskq->ntasks[priolevel]--;
				taskq->total_ntasks--;
				STARPU_TRACE_JOB_POP(task, 0);
			}
		} while (!task && priolevel-- > 0);
	}

	PTHREAD_MUTEX_UNLOCK(&global_sched_mutex);

	return task;
}

struct starpu_sched_policy_s _starpu_sched_prio_policy = {
	.init_sched = initialize_eager_center_priority_policy,
	.deinit_sched = deinitialize_eager_center_priority_policy,
	/* we always use priorities in that policy */
	.push_task = _starpu_priority_push_task,
	.push_prio_task = _starpu_priority_push_task,
	.pop_task = _starpu_priority_pop_task,
	.post_exec_hook = NULL,
	.pop_every_task = NULL,
	.policy_name = "prio",
	.policy_description = "eager (with priorities)"
};
