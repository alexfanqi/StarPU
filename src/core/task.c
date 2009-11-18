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
#include <core/workers.h>
#include <core/jobs.h>
#include <core/task.h>
#include <common/config.h>

/* XXX this should be reinitialized when StarPU is shutdown (or we should make
 * sure that no task remains !) */
/* TODO we could make this hierarchical to avoid contention ? */
static pthread_cond_t submitted_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t submitted_mutex = PTHREAD_MUTEX_INITIALIZER;
static long int nsubmitted = 0;

void starpu_task_init(struct starpu_task *task)
{
	STARPU_ASSERT(task);

	task->cl = NULL;
	task->cl_arg = NULL;
	task->cl_arg_size = 0;

	task->callback_func = NULL;
	task->callback_arg = NULL;

	task->priority = DEFAULT_PRIO;
	task->use_tag = 0;
	task->synchronous = 0;

	task->execute_on_a_specific_worker = 0;

	task->detach = 1;

	/* by default, we do not let StarPU free the task structure since
	 * starpu_task_init is likely to be used only for statically allocated
	 * tasks */
	task->destroy = 0;

	task->starpu_private = NULL;
}

struct starpu_task * __attribute__((malloc)) starpu_task_create(void)
{
	struct starpu_task *task;

	task = calloc(1, sizeof(struct starpu_task));
	STARPU_ASSERT(task);

	starpu_task_init(task);

	/* Dynamically allocated tasks are destroyed by default */
	task->destroy = 1;

	return task;
}

/* Liberate the ressource allocated during starpu_task_create. This function
 * can be called automatically after the execution of a task by setting the
 * "destroy" flag of the starpu_task structure (default behaviour). Calling
 * this function on a statically allocated task results in an undefined
 * behaviour. */
void starpu_task_destroy(struct starpu_task *task)
{
	STARPU_ASSERT(task);

	/* TODO handle the case of task with detach = 1 and destroy = 1 */
	/* TODO handle the case of non terminated tasks -> return -EINVAL */
	
	free(task);
}

int starpu_wait_task(struct starpu_task *task)
{
	STARPU_ASSERT(task);

	if (task->detach || task->synchronous)
		return -EINVAL;

	job_t j = (struct job_s *)task->starpu_private;

	starpu_wait_job(j);

	/* as this is a synchronous task, the liberation of the job
	   structure was deferred */
	if (task->destroy)
		free(task);

	return 0;
}

/* application should submit new tasks to StarPU through this function */
int starpu_submit_task(struct starpu_task *task)
{
	int ret;
	unsigned is_sync = task->synchronous;

#warning synchronous tasks should perhaps be deprecated instead !
	if (is_sync)
		task->detach = 0;

	STARPU_ASSERT(task);

	if (task->cl && !worker_exists(task->cl->where))
		return -ENODEV;

	/* internally, StarPU manipulates a job_t which is a wrapper around a
 	* task structure */
	job_t j = job_create(task);

	task->starpu_private = j;

	increment_nsubmitted_tasks();

	ret = enforce_deps_and_schedule(j);

	/* XXX modify when we'll have starpu_wait_task */
	if (is_sync)
		starpu_wait_job(j);

	return ret;
}

/* This function is supplied for convenience only, it is equivalent to setting
 * the proper flag and submitting the task with submit_task.
 * Note that this call is blocking, and will not make StarPU progress,
 * so it must only be called from the programmer thread, not by StarPU.
 * NB: This also means that it cannot be submitted within a callback ! */
int submit_sync_task(struct starpu_task *task)
{
	task->synchronous = 1;

	return starpu_submit_task(task);
}

void starpu_display_codelet_stats(struct starpu_codelet_t *cl)
{
	unsigned worker;
	unsigned nworkers = starpu_get_worker_count();

	if (cl->model && cl->model->symbol)
		fprintf(stderr, "Statistics for codelet %s\n", cl->model->symbol);

	unsigned long total = 0;
	
	for (worker = 0; worker < nworkers; worker++)
		total += cl->per_worker_stats[worker];

	for (worker = 0; worker < nworkers; worker++)
	{
		char name[32];
		starpu_get_worker_name(worker, name, 32);

		fprintf(stderr, "\t%s -> %ld / %ld (%2.2f \%%)\n", name, cl->per_worker_stats[worker], total, (100.0f*cl->per_worker_stats[worker])/total);
	}
}

void starpu_wait_all_tasks(void)
{
	int res;

	pthread_mutex_lock(&submitted_mutex);

	if (nsubmitted > 0)
	{
		res = pthread_cond_wait(&submitted_cond, &submitted_mutex);
		STARPU_ASSERT(!res);
	}

	
	pthread_mutex_unlock(&submitted_mutex);
}

void decrement_nsubmitted_tasks(void)
{
	pthread_mutex_lock(&submitted_mutex);
	if (--nsubmitted == 0)
	{
		int broadcast_res;
		broadcast_res = pthread_cond_broadcast(&submitted_cond);
		STARPU_ASSERT(!broadcast_res);

	}
	pthread_mutex_unlock(&submitted_mutex);

}

void increment_nsubmitted_tasks(void)
{
	pthread_mutex_lock(&submitted_mutex);
	nsubmitted++;
	pthread_mutex_unlock(&submitted_mutex);
}