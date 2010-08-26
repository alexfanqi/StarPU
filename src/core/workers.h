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

#ifndef __WORKERS_H__
#define __WORKERS_H__

#include <starpu.h>
#include <common/config.h>
#include <pthread.h>
#include <common/timing.h>
#include <common/fxt.h>
#include <core/jobs.h>
#include <core/perfmodel/perfmodel.h>
#include <core/policies/sched_policy.h>
#include <core/topology.h>
#include <core/errorcheck.h>


#ifdef STARPU_HAVE_HWLOC
#include <hwloc.h>
#endif

#ifdef STARPU_USE_CUDA
#include <drivers/cuda/driver_cuda.h>
#endif

#ifdef STARPU_USE_OPENCL
#include <drivers/opencl/driver_opencl.h>
#endif

#ifdef STARPU_USE_GORDON
#include <drivers/gordon/driver_gordon.h>
#endif

#include <drivers/cpu/driver_cpu.h>

#include <datawizard/datawizard.h>

#define STARPU_CPU_ALPHA	1.0f
#define STARPU_CUDA_ALPHA	13.33f
#define STARPU_OPENCL_ALPHA	12.22f
#define STARPU_GORDON_ALPHA	6.0f /* XXX this is a random value ... */

struct starpu_worker_s {
	struct starpu_machine_config_s *config;
        pthread_mutex_t mutex;
	enum starpu_archtype arch; /* what is the type of worker ? */
	uint32_t worker_mask; /* what is the type of worker ? */
	enum starpu_perf_archtype perf_arch; /* in case there are different models of the same arch */
	pthread_t worker_thread; /* the thread which runs the worker */
	int devid; /* which cpu/gpu/etc is controlled by the workker ? */
	int bindid; /* which cpu is the driver bound to ? */
	int workerid; /* uniquely identify the worker among all processing units types */
        pthread_cond_t ready_cond; /* indicate when the worker is ready */
	unsigned memory_node; /* which memory node is associated that worker to ? */
	struct starpu_jobq_s *jobq; /* in which queue will that worker get/put tasks ? */
	struct starpu_job_list_s *local_jobs; /* this queue contains tasks that have been explicitely submitted to that queue */
	pthread_mutex_t local_jobs_mutex; /* protect the local_jobs list */
	struct starpu_worker_set_s *set; /* in case this worker belongs to a set */
	struct starpu_job_list_s *terminated_jobs; /* list of pending jobs which were executed */
	unsigned worker_is_running;
	unsigned worker_is_initialized;
	starpu_worker_status status; /* what is the worker doing now ? (eg. CALLBACK) */
	char name[32];
};

/* in case a single CPU worker may control multiple 
 * accelerators (eg. Gordon for n SPUs) */
struct starpu_worker_set_s {
        pthread_mutex_t mutex;
	pthread_t worker_thread; /* the thread which runs the worker */
	unsigned nworkers;
	unsigned joined; /* only one thread may call pthread_join*/
	void *retval;
	struct starpu_worker_s *workers;
        pthread_cond_t ready_cond; /* indicate when the set is ready */
	unsigned set_is_initialized;
};

struct starpu_machine_config_s {
	unsigned nworkers;

#ifdef STARPU_HAVE_HWLOC
	hwloc_topology_t hwtopology;
	int cpu_depth;
#endif

	unsigned nhwcpus;
        unsigned nhwcudagpus;
        unsigned nhwopenclgpus;

	unsigned ncpus;
	unsigned ncudagpus;
	unsigned nopenclgpus;
	unsigned ngordon_spus;

	/* Where to bind workers ? */
	int current_bindid;
	unsigned workers_bindid[STARPU_NMAXWORKERS];
	
	/* Which GPU(s) do we use for CUDA ? */
	int current_cuda_gpuid;
	unsigned workers_cuda_gpuid[STARPU_NMAXWORKERS];

	/* Which GPU(s) do we use for OpenCL ? */
	int current_opencl_gpuid;
	unsigned workers_opencl_gpuid[STARPU_NMAXWORKERS];
	
	struct starpu_worker_s workers[STARPU_NMAXWORKERS];

	/* This bitmask indicates which kinds of worker are available. For
	 * instance it is possible to test if there is a CUDA worker with
	 * the result of (worker_mask & STARPU_CUDA). */
	uint32_t worker_mask;

	/* in case the user gives an explicit configuration, this is only valid
	 * during starpu_init. */
	struct starpu_conf *user_conf;

	/* this flag is set until the runtime is stopped */
	unsigned running;
};

unsigned _starpu_machine_is_running(void);

uint32_t _starpu_worker_exists(uint32_t task_mask);
uint32_t _starpu_may_submit_cuda_task(void);
uint32_t _starpu_may_submit_cpu_task(void);
uint32_t _starpu_may_submit_opencl_task(void);
uint32_t _starpu_worker_may_execute_task(unsigned workerid, uint32_t where);
unsigned _starpu_worker_can_block(unsigned memnode);
void _starpu_block_worker(int workerid, pthread_cond_t *cond, pthread_mutex_t *mutex);

void _starpu_set_local_worker_key(struct starpu_worker_s *worker);
struct starpu_worker_s *_starpu_get_local_worker_key(void);

struct starpu_worker_s *_starpu_get_worker_struct(unsigned id);

struct starpu_machine_config_s *_starpu_get_machine_config(void);

starpu_worker_status _starpu_worker_get_status(int workerid);
void _starpu_worker_set_status(int workerid, starpu_worker_status status);

/* TODO move */
unsigned _starpu_execute_registered_progression_hooks(void);

#endif // __WORKERS_H__
