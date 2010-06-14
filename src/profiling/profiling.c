/*
 * StarPU
 * Copyright (C) INRIA 2008-2010 (see AUTHORS file)
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
#include <starpu_profiling.h>
#include <profiling/profiling.h>
#include <core/workers.h>
#include <common/config.h>
#include <common/utils.h>
#include <common/timing.h>
#include <common/fxt.h>
#include <errno.h>

static struct starpu_worker_profiling_info worker_info[STARPU_NMAXWORKERS];
static pthread_mutex_t worker_info_mutex[STARPU_NMAXWORKERS];

/* In case the worker is still sleeping when the user request profiling info,
 * we need to account for the time elasped while sleeping. */
static unsigned worker_registered_sleeping_start[STARPU_NMAXWORKERS];
static struct timespec sleeping_start_date[STARPU_NMAXWORKERS];

static unsigned worker_registered_executing_start[STARPU_NMAXWORKERS];
static struct timespec executing_start_date[STARPU_NMAXWORKERS];


/* Store the busid of the different (src, dst) pairs. busid_matrix[src][dst]
 * contains the busid of (src, dst) or -1 if the bus was not registered. */
struct node_pair {
	int src;
	int dst;
	struct starpu_bus_profiling_info *bus_info;
};

static int busid_matrix[STARPU_MAXNODES][STARPU_MAXNODES];
static struct starpu_bus_profiling_info bus_profiling_info[STARPU_MAXNODES][STARPU_MAXNODES];
static struct node_pair busid_to_node_pair[STARPU_MAXNODES*STARPU_MAXNODES]; 
static int busid_cnt = 0;

static void _do_starpu_bus_reset_profiling_info(struct starpu_bus_profiling_info *bus_info);

/*
 *	Global control of profiling
 */

/* Disabled by default */
static int profiling = 0;

int starpu_profiling_status_set(int status)
{
	int prev_value = profiling;
	profiling = status;

	STARPU_TRACE_SET_PROFILING(status);

	/* If we enable profiling, we reset the counters. */
	if (status == STARPU_PROFILING_ENABLE)
	{
		int worker;
		for (worker = 0; worker < STARPU_NMAXWORKERS; worker++)
			_starpu_worker_reset_profiling_info(worker);

		int busid;
		int bus_cnt = starpu_bus_get_count();
		for (busid = 0; busid < bus_cnt; busid++)
		{
			struct starpu_bus_profiling_info *bus_info;
			bus_info = busid_to_node_pair[busid].bus_info;

			_do_starpu_bus_reset_profiling_info(bus_info);;
		}
	}

	return prev_value;
}

int starpu_profiling_status_get(void)
{
	return profiling;
}

void starpu_profiling_init(void)
{
	int worker;
	for (worker = 0; worker < STARPU_NMAXWORKERS; worker++)
	{
		PTHREAD_MUTEX_INIT(&worker_info_mutex[worker], NULL);
		_starpu_worker_reset_profiling_info(worker);
	}
}

void starpu_profiling_terminate(void)
{

}

/*
 *	Task profiling
 */

struct starpu_task_profiling_info *_starpu_allocate_profiling_info_if_needed(void)
{
	struct starpu_task_profiling_info *info = NULL;

	if (profiling)
	{
		info = calloc(1, sizeof(struct starpu_task_profiling_info));
		STARPU_ASSERT(info);

		starpu_timespec_clear(&info->submit_time);
		starpu_timespec_clear(&info->start_time);
		starpu_timespec_clear(&info->end_time);
	}

	return info;
}

/*
 *	Worker profiling
 */

static void _do_starpu_worker_reset_profiling_info(int workerid)
{
	starpu_clock_gettime(&worker_info[workerid].start_time);

	/* This is computed in a lazy fashion when the application queries
	 * profiling info. */
	starpu_timespec_clear(&worker_info[workerid].total_time);

	starpu_timespec_clear(&worker_info[workerid].executing_time);
	starpu_timespec_clear(&worker_info[workerid].sleeping_time);

	worker_info[workerid].executed_tasks = 0;
	
	/* We detect if the worker is already sleeping or doing some
	 * computation */
	starpu_worker_status status = _starpu_worker_get_status(workerid);

	if (status == STATUS_SLEEPING)
	{
		worker_registered_sleeping_start[workerid] = 1;
		starpu_clock_gettime(&sleeping_start_date[workerid]);
	}
	else {
		worker_registered_sleeping_start[workerid] = 0;
	}

	if (status == STATUS_EXECUTING)
	{
		worker_registered_executing_start[workerid] = 1;
		starpu_clock_gettime(&executing_start_date[workerid]);
	}
	else {
		worker_registered_executing_start[workerid] = 0;
	}
}

void _starpu_worker_reset_profiling_info(int workerid)
{
	PTHREAD_MUTEX_LOCK(&worker_info_mutex[workerid]);
	_do_starpu_worker_reset_profiling_info(workerid);
	PTHREAD_MUTEX_UNLOCK(&worker_info_mutex[workerid]);
}

void _starpu_worker_register_sleeping_start_date(int workerid, struct timespec *sleeping_start)
{
	if (profiling)
	{
		PTHREAD_MUTEX_LOCK(&worker_info_mutex[workerid]);
		worker_registered_sleeping_start[workerid] = 1;	
		memcpy(&sleeping_start_date[workerid], sleeping_start, sizeof(struct timespec));
		PTHREAD_MUTEX_UNLOCK(&worker_info_mutex[workerid]);
	}
}

void _starpu_worker_register_executing_start_date(int workerid, struct timespec *executing_start)
{
	if (profiling)
	{
		PTHREAD_MUTEX_LOCK(&worker_info_mutex[workerid]);
		worker_registered_executing_start[workerid] = 1;	
		memcpy(&executing_start_date[workerid], executing_start, sizeof(struct timespec));
		PTHREAD_MUTEX_UNLOCK(&worker_info_mutex[workerid]);
	}
}

void _starpu_worker_update_profiling_info_sleeping(int workerid, struct timespec *sleeping_start, struct timespec *sleeping_end)
{
	if (profiling)
	{
		PTHREAD_MUTEX_LOCK(&worker_info_mutex[workerid]);

                /* Perhaps that profiling was enabled while the worker was
                 * already blocked, so we don't measure (end - start), but 
                 * (end - max(start,worker_start)) where worker_start is the
                 * date of the previous profiling info reset on the worker */
		struct timespec *worker_start = &worker_info[workerid].start_time;
		if (starpu_timespec_cmp(sleeping_start, worker_start, <))
		{
			/* sleeping_start < worker_start */
			sleeping_start = worker_start;
		}

		struct timespec sleeping_time;
		starpu_timespec_sub(sleeping_end, sleeping_start, &sleeping_time);

		starpu_timespec_accumulate(&worker_info[workerid].sleeping_time, &sleeping_time);

		worker_registered_sleeping_start[workerid] = 0;	

		PTHREAD_MUTEX_UNLOCK(&worker_info_mutex[workerid]);
	}
}


void _starpu_worker_update_profiling_info_executing(int workerid, struct timespec *executing_time, int executed_tasks)
{
	if (profiling)
	{
		PTHREAD_MUTEX_LOCK(&worker_info_mutex[workerid]);

		starpu_timespec_accumulate(&worker_info[workerid].executing_time, executing_time);

		worker_info[workerid].executed_tasks += executed_tasks;
	
		PTHREAD_MUTEX_UNLOCK(&worker_info_mutex[workerid]);
	}
}

int starpu_worker_get_profiling_info(int workerid, struct starpu_worker_profiling_info *info)
{
	if (!profiling)
		return -EINVAL;

	PTHREAD_MUTEX_LOCK(&worker_info_mutex[workerid]);

	if (info)
	{
		/* The total time is computed in a lazy fashion */
		struct timespec now;
		starpu_clock_gettime(&now);

		/* In case some worker is currently sleeping, we take into
		 * account the time spent since it registered. */
		if (worker_registered_sleeping_start[workerid])
		{
			struct timespec sleeping_time;
			starpu_timespec_sub(&now, &sleeping_start_date[workerid], &sleeping_time);
			starpu_timespec_accumulate(&worker_info[workerid].sleeping_time, &sleeping_time);
		}

		if (worker_registered_executing_start[workerid])
		{
			struct timespec executing_time;
			starpu_timespec_sub(&now, &executing_start_date[workerid], &executing_time);
			starpu_timespec_accumulate(&worker_info[workerid].executing_time, &executing_time);
		}

		/* total_time = now - start_time */
		starpu_timespec_sub(&now, &worker_info[workerid].start_time,
					&worker_info[workerid].total_time);

		memcpy(info, &worker_info[workerid], sizeof(struct starpu_worker_profiling_info));
	}

	_do_starpu_worker_reset_profiling_info(workerid);

	PTHREAD_MUTEX_UNLOCK(&worker_info_mutex[workerid]);

	return 0;
}

/*
 *	Bus profiling
 */

void _starpu_initialize_busid_matrix(void)
{
	int i, j;
	for (j = 0; j < STARPU_MAXNODES; j++)
	for (i = 0; i < STARPU_MAXNODES; i++)
		busid_matrix[i][j] = -1;	

	busid_cnt = 0;
}

static void _do_starpu_bus_reset_profiling_info(struct starpu_bus_profiling_info *bus_info)
{
	starpu_clock_gettime(&bus_info->start_time);
	bus_info->transferred_bytes = 0;
	bus_info->transfer_count = 0;
}

int _starpu_register_bus(int src_node, int dst_node)
{
	if (busid_matrix[src_node][dst_node] != -1)
		return -EBUSY;

	int busid = STARPU_ATOMIC_ADD(&busid_cnt, 1) - 1;

	busid_matrix[src_node][dst_node] = busid;

	busid_to_node_pair[busid].src = src_node;
	busid_to_node_pair[busid].dst = dst_node;
	busid_to_node_pair[busid].bus_info = &bus_profiling_info[src_node][dst_node];

	_do_starpu_bus_reset_profiling_info(&bus_profiling_info[src_node][dst_node]);

	return busid;
}

int starpu_bus_get_count(void)
{
	return busid_cnt;
}

int starpu_bus_get_id(int src, int dst)
{
	return busid_matrix[src][dst];
}

int starpu_bus_get_src(int busid)
{
	return busid_to_node_pair[busid].src;
}

int starpu_bus_get_dst(int busid)
{
	return busid_to_node_pair[busid].dst;
}

int starpu_bus_get_profiling_info(int busid, struct starpu_bus_profiling_info *bus_info)
{
	int src_node = busid_to_node_pair[busid].src;
	int dst_node = busid_to_node_pair[busid].dst;

	/* XXX protect all this  method with a mutex */
	if (bus_info)
	{
		struct timespec now;
		starpu_clock_gettime(&now);

		/* total_time = now - start_time */
		starpu_timespec_sub(&now, &bus_profiling_info[src_node][dst_node].start_time,
					  &bus_profiling_info[src_node][dst_node].total_time);

		memcpy(bus_info, &bus_profiling_info[src_node][dst_node], sizeof(struct starpu_bus_profiling_info));
	}

	_do_starpu_bus_reset_profiling_info(&bus_profiling_info[src_node][dst_node]);

	return 0;
} 

void _starpu_bus_update_profiling_info(int src_node, int dst_node, size_t size)
{
	bus_profiling_info[src_node][dst_node].transferred_bytes += size;
	bus_profiling_info[src_node][dst_node].transfer_count++;
//	fprintf(stderr, "PROFILE %d -> %d : %d (cnt %d)\n", src_node, dst_node, size, bus_profiling_info[src_node][dst_node].transfer_count);
}
