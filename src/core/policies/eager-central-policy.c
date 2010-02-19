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

#include <core/policies/eager-central-policy.h>

/*
 *	This is just the trivial policy where every worker use the same
 *	JOB QUEUE.
 */

/* the former is the actual queue, the latter some container */
static struct jobq_s *jobq;

static void init_central_queue_design(void)
{
	/* there is only a single queue in that trivial design */
	jobq = _starpu_create_fifo();

	_starpu_init_fifo_queues_mechanisms();

	jobq->_starpu_push_task = _starpu_fifo_push_task;
	jobq->push_prio_task = _starpu_fifo_push_prio_task;
	jobq->_starpu_pop_task = _starpu_fifo_pop_task;

	jobq->_starpu_pop_every_task = _starpu_fifo_pop_every_task;
}

static struct jobq_s *func_init_central_queue(void)
{
	/* once again, this is trivial */
	return jobq;
}

static void initialize_eager_center_policy(struct starpu_machine_config_s *config, 
		   __attribute__ ((unused)) struct starpu_sched_policy_s *_policy) 
{
	setup_queues(init_central_queue_design, func_init_central_queue, config);
}

static struct jobq_s *get_local_queue_eager(struct starpu_sched_policy_s *policy 
						__attribute__ ((unused)))
{
	/* this is trivial for that strategy :) */
	return jobq;
}

struct starpu_sched_policy_s sched_eager_policy = {
	.init_sched = initialize_eager_center_policy,
	.deinit_sched = NULL,
	.get_local_queue = get_local_queue_eager,
	.policy_name = "eager",
	.policy_description = "greedy policy"
};
