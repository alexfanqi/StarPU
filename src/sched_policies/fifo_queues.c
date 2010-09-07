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

/* FIFO queues, ready for use by schedulers */

#include <pthread.h>
#include <sched_policies/fifo_queues.h>
#include <errno.h>
#include <common/utils.h>
#include <core/task.h>

struct starpu_fifo_taskq_s *_starpu_create_fifo(void)
{
	struct starpu_fifo_taskq_s *fifo;
	fifo = malloc(sizeof(struct starpu_fifo_taskq_s));

	/* note that not all mechanisms (eg. the semaphore) have to be used */
	starpu_task_list_init(&fifo->taskq);
	fifo->ntasks = 0;
	fifo->nprocessed = 0;

	fifo->exp_start = _starpu_timing_now();
	fifo->exp_len = 0.0;
	fifo->exp_end = fifo->exp_start;

	return fifo;
}

void _starpu_destroy_fifo(struct starpu_fifo_taskq_s *fifo)
{
	free(fifo);
}

int _starpu_fifo_push_prio_task(struct starpu_fifo_taskq_s *fifo_queue, pthread_mutex_t *sched_mutex, pthread_cond_t *sched_cond, struct starpu_task *task)
{
	PTHREAD_MUTEX_LOCK(sched_mutex);

	STARPU_TRACE_JOB_PUSH(task, 0);
	starpu_task_list_push_back(&fifo_queue->taskq, task);
	fifo_queue->ntasks++;
	fifo_queue->nprocessed++;

	pthread_cond_signal(sched_cond);
	PTHREAD_MUTEX_UNLOCK(sched_mutex);

	return 0;
}

int _starpu_fifo_push_task(struct starpu_fifo_taskq_s *fifo_queue, pthread_mutex_t *sched_mutex, pthread_cond_t *sched_cond, struct starpu_task *task)
{
	PTHREAD_MUTEX_LOCK(sched_mutex);

	STARPU_TRACE_JOB_PUSH(task, 0);
	starpu_task_list_push_front(&fifo_queue->taskq, task);
	fifo_queue->ntasks++;
	fifo_queue->nprocessed++;

	pthread_cond_signal(sched_cond);
	PTHREAD_MUTEX_UNLOCK(sched_mutex);

	return 0;
}

struct starpu_task *_starpu_fifo_pop_task(struct starpu_fifo_taskq_s *fifo_queue)
{
	struct starpu_task *task = NULL;

	if (fifo_queue->ntasks == 0)
		return NULL;

	if (fifo_queue->ntasks > 0) 
	{
		/* there is a task */
		task = starpu_task_list_pop_back(&fifo_queue->taskq);
	
		STARPU_ASSERT(task);
		fifo_queue->ntasks--;
		
		STARPU_TRACE_JOB_POP(task, 0);
	}
	
	return task;
}

/* pop every task that can be executed on the calling driver */
struct starpu_task *_starpu_fifo_pop_every_task(struct starpu_fifo_taskq_s *fifo_queue, pthread_mutex_t *sched_mutex, uint32_t where)
{
	struct starpu_task_list *old_list;
	unsigned size;

	struct starpu_task *new_list = NULL;
	struct starpu_task *new_list_tail = NULL;
	
	PTHREAD_MUTEX_LOCK(sched_mutex);

	size = fifo_queue->ntasks;

	if (size > 0) {
		old_list = &fifo_queue->taskq;
		unsigned new_list_size = 0;

		struct starpu_task *task, *next_task;
		/* note that this starts at the _head_ of the list, so we put
 		 * elements at the back of the new list */
		task = starpu_task_list_front(old_list);
		while (task)
		{
			next_task = task->next;

			if (task->cl->where & where)
			{
				/* this elements can be moved into the new list */
				new_list_size++;
				
				starpu_task_list_erase(old_list, task);

				if (new_list_tail)
				{
					new_list_tail->next = task;
					task->prev = new_list_tail;
					task->next = NULL;
					new_list_tail = task;
				}
				else {
					new_list = task;
					new_list_tail = task;
					task->prev = NULL;
					task->next = NULL;
				}
			}
		
			task = next_task;
		}

		fifo_queue->ntasks -= new_list_size;
	}

	PTHREAD_MUTEX_UNLOCK(sched_mutex);

	return new_list;
}
