
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

#include <math.h>
#include <starpu.h>
#include <starpu_profiling.h>
#include <common/config.h>
#include <common/utils.h>
#include <core/debug.h>
#include <starpu_opencl.h>
#include <drivers/driver_common/driver_common.h>
#include "driver_opencl.h"
#include "driver_opencl_utils.h"
#include <common/utils.h>
#include <profiling/profiling.h>

static cl_context contexts[STARPU_MAXOPENCLDEVS];
static cl_device_id devices[STARPU_MAXOPENCLDEVS];
static cl_command_queue queues[STARPU_MAXOPENCLDEVS];
static cl_uint nb_devices = -1;
static int init_done = 0;
extern char *_starpu_opencl_program_dir;

void starpu_opencl_get_context(int devid, cl_context *context)
{
        *context = contexts[devid];
}

void starpu_opencl_get_device(int devid, cl_device_id *device)
{
        *device = devices[devid];
}

void starpu_opencl_get_queue(int devid, cl_command_queue *queue)
{
        *queue = queues[devid];
}

int _starpu_opencl_init_context(int devid)
{
	cl_int err;
        cl_device_id device;

        _STARPU_OPENCL_DEBUG("Initialising context for dev %d\n", devid);

        // Create a compute context
        device = devices[devid];
        contexts[devid] = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
        if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

        // Create queue for the given device
        queues[devid] = clCreateCommandQueue(contexts[devid], devices[devid], 0, &err);
        if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

	return EXIT_SUCCESS;
}

int _starpu_opencl_deinit_context(int devid)
{
        int err;

        _STARPU_OPENCL_DEBUG("De-initialising context for dev %d\n", devid);

        err = clReleaseContext(contexts[devid]);
        if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

        err = clReleaseCommandQueue(queues[devid]);
        if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

        return EXIT_SUCCESS;
}

int _starpu_opencl_allocate_memory(void **addr, size_t size, cl_mem_flags flags)
{
	cl_int err;
        cl_mem address;
        struct starpu_worker_s *worker = _starpu_get_local_worker_key();

	address = clCreateBuffer(contexts[worker->devid], flags, size, NULL, &err);
	if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

        *addr = address;
        return EXIT_SUCCESS;
}

int _starpu_opencl_copy_ram_to_opencl_async_sync(void *ptr, cl_mem buffer, size_t size, size_t offset, cl_event *event, int *ret)
{
        int err;
        struct starpu_worker_s *worker = _starpu_get_local_worker_key();
        cl_bool blocking;

        blocking = (event == NULL) ? CL_TRUE : CL_FALSE;
        err = clEnqueueWriteBuffer(queues[worker->devid], buffer, blocking, offset, size, ptr, 0, NULL, event);
        if (STARPU_LIKELY(err == CL_SUCCESS)) {
                *ret = (event == NULL) ? 0 : EAGAIN;
                return EXIT_SUCCESS;
        }
        else {
                if (event != NULL)
                        /* The asynchronous copy has failed, try to copy synchronously */
                        err = clEnqueueWriteBuffer(queues[worker->devid], buffer, CL_TRUE, offset, size, ptr, 0, NULL, NULL);
                if (STARPU_LIKELY(err == CL_SUCCESS)) {
                        *ret = 0;
                        return EXIT_SUCCESS;
                }
                else {
                        STARPU_OPENCL_REPORT_ERROR(err);
                        return err;
                }
        }
}

int _starpu_opencl_copy_ram_to_opencl(void *ptr, cl_mem buffer, size_t size, size_t offset, cl_event *event)
{
        int err;
        struct starpu_worker_s *worker = _starpu_get_local_worker_key();
        cl_bool blocking;

        blocking = (event == NULL) ? CL_TRUE : CL_FALSE;
        err = clEnqueueWriteBuffer(queues[worker->devid], buffer, blocking, offset, size, ptr, 0, NULL, event);
        if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

        return EXIT_SUCCESS;
}

int _starpu_opencl_copy_opencl_to_ram_async_sync(cl_mem buffer, void *ptr, size_t size, size_t offset, cl_event *event, int *ret)
{
        int err;
        struct starpu_worker_s *worker = _starpu_get_local_worker_key();
        cl_bool blocking;

        blocking = (event == NULL) ? CL_TRUE : CL_FALSE;
        err = clEnqueueReadBuffer(queues[worker->devid], buffer, blocking, offset, size, ptr, 0, NULL, event);
        if (STARPU_LIKELY(err == CL_SUCCESS)) {
                *ret = (event == NULL) ? 0 : EAGAIN;
                return EXIT_SUCCESS;
        }
        else {
                if (event != NULL)
                        /* The asynchronous copy has failed, try to copy synchronously */
                        err = clEnqueueReadBuffer(queues[worker->devid], buffer, CL_TRUE, offset, size, ptr, 0, NULL, NULL);
                if (STARPU_LIKELY(err == CL_SUCCESS)) {
                        *ret = 0;
                        return EXIT_SUCCESS;
                }
                else {
                        STARPU_OPENCL_REPORT_ERROR(err);
                        return err;
                }
        }

        return EXIT_SUCCESS;
}

int _starpu_opencl_copy_opencl_to_ram(cl_mem buffer, void *ptr, size_t size, size_t offset, cl_event *event)
{
        int err;
        struct starpu_worker_s *worker = _starpu_get_local_worker_key();
        cl_bool blocking;

        blocking = (event == NULL) ? CL_TRUE : CL_FALSE;
        err = clEnqueueReadBuffer(queues[worker->devid], buffer, blocking, offset, size, ptr, 0, NULL, event);
        if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

        return EXIT_SUCCESS;
}

void _starpu_opencl_init(void)
{
        if (!init_done) {
                cl_platform_id platform_id[STARPU_OPENCL_PLATFORM_MAX];
                cl_uint nb_platforms;
                cl_device_type device_type = CL_DEVICE_TYPE_GPU;
                cl_int err;
                unsigned int i;

                _STARPU_OPENCL_DEBUG("Initialising OpenCL\n");

                // Get Platforms
                err = clGetPlatformIDs(STARPU_OPENCL_PLATFORM_MAX, platform_id, &nb_platforms);
                if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);
                _STARPU_OPENCL_DEBUG("Platforms detected: %d\n", nb_platforms);

                // Get devices
                nb_devices = 0;
                {
                        for (i=0; i<nb_platforms; i++) {
                                cl_uint num;

#ifdef STARPU_VERBOSE
                                {
                                        char name[1024], vendor[1024];
                                        clGetPlatformInfo(platform_id[i], CL_PLATFORM_NAME, 1024, name, NULL);
                                        clGetPlatformInfo(platform_id[i], CL_PLATFORM_VENDOR, 1024, vendor, NULL);
                                        _STARPU_OPENCL_DEBUG("Platform: %s - %s\n", name, vendor);
                                }
#endif
                                err = clGetDeviceIDs(platform_id[i], device_type, STARPU_MAXOPENCLDEVS-nb_devices, &devices[nb_devices], &num);
                                if (err == CL_DEVICE_NOT_FOUND) {
                                        _STARPU_OPENCL_DEBUG("  No devices detected on this platform\n");
                                }
                                else {
                                        if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);
                                        _STARPU_OPENCL_DEBUG("  %d devices detected\n", num);
                                        nb_devices += num;
                                }
                        }
                }

                // Get location of OpenCl codelet source files
                _starpu_opencl_program_dir = getenv("STARPU_OPENCL_PROGRAM_DIR");

                // initialise internal structures
                for(i=0 ; i<nb_devices ; i++) {
                        contexts[i] = NULL;
                        queues[i] = NULL;
                }

                init_done=1;
        }
}

static unsigned _starpu_opencl_get_device_name(int dev, char *name, int lname);
static int _starpu_opencl_execute_job(starpu_job_t j, struct starpu_worker_s *args);

void *_starpu_opencl_worker(void *arg)
{
	struct starpu_worker_s* args = arg;
	struct starpu_jobq_s *jobq = args->jobq;

	int devid = args->devid;
	int workerid = args->workerid;

#ifdef USE_FXT
	fxt_register_thread(args->bindid);
#endif

	unsigned memnode = args->memory_node;
	STARPU_TRACE_WORKER_INIT_START(STARPU_FUT_OPENCL_KEY, devid, memnode);

	_starpu_bind_thread_on_cpu(args->config, args->bindid);

	_starpu_set_local_memory_node_key(&memnode);

	_starpu_set_local_queue(jobq);

	_starpu_set_local_worker_key(args);

	_starpu_opencl_init_context(devid);

	/* one more time to avoid hacks from third party lib :) */
	_starpu_bind_thread_on_cpu(args->config, args->bindid);

	args->status = STATUS_UNKNOWN;

	/* get the device's name */
	char devname[128];
	_starpu_opencl_get_device_name(devid, devname, 128);
	snprintf(args->name, 32, "OpenCL %d (%s)", args->devid, devname);

	_STARPU_OPENCL_DEBUG("OpenCL (%s) dev id %d thread is ready to run on CPU %d !\n", devname, devid, args->bindid);

	STARPU_TRACE_WORKER_INIT_END

	/* tell the main thread that this one is ready */
	PTHREAD_MUTEX_LOCK(&args->mutex);
	args->worker_is_initialized = 1;
	PTHREAD_COND_SIGNAL(&args->ready_cond);
	PTHREAD_MUTEX_UNLOCK(&args->mutex);

	struct starpu_job_s * j;
	int res;

	struct starpu_sched_policy_s *policy = _starpu_get_sched_policy();
	struct starpu_jobq_s *queue = policy->starpu_get_local_queue(policy);

	while (_starpu_machine_is_running())
	{
		STARPU_TRACE_START_PROGRESS(memnode);
		_starpu_datawizard_progress(memnode, 1);
		STARPU_TRACE_END_PROGRESS(memnode);

		_starpu_execute_registered_progression_hooks();

		_starpu_jobq_lock(queue);

		/* perhaps there is some local task to be executed first */
		j = _starpu_pop_local_task(args);

		/* otherwise ask a task to the scheduler */
		if (!j)
			j = _starpu_pop_task();
		
                if (j == NULL) 
		{
			if (_starpu_worker_can_block(memnode))
				_starpu_block_worker(workerid, &queue->activity_cond, &queue->activity_mutex);

			_starpu_jobq_unlock(queue);

			continue;
		};

		_starpu_jobq_unlock(queue);
	       
		/* can OpenCL do that task ? */
		if (!STARPU_OPENCL_MAY_PERFORM(j))
		{
			/* this is not a OpenCL task */
			_starpu_push_task(j, 0);
			continue;
		}

		_starpu_set_current_task(j->task);

		res = _starpu_opencl_execute_job(j, args);

		_starpu_set_current_task(NULL);

                if (res) {
			switch (res) {
				case -EAGAIN:
					fprintf(stderr, "ouch, put the codelet %p back ... \n", j);
					_starpu_push_task(j, 0);
					STARPU_ABORT();
					continue;
				default:
					assert(0);
			}
		}

		_starpu_handle_job_termination(j, 0);
	}

	STARPU_TRACE_WORKER_DEINIT_START

          _starpu_opencl_deinit_context(devid);

#ifdef STARPU_DATA_STATS
	fprintf(stderr, "OpenCL #%d computation %le comm %le (%lf \%%)\n", args->id, jobq->total_computation_time, jobq->total_communication_time, jobq->total_communication_time*100.0/jobq->total_computation_time);
#endif

#ifdef STARPU_VERBOSE
	double ratio = 0;
	if (jobq->total_job_performed != 0)
	{
		ratio = jobq->total_computation_time_error/jobq->total_computation_time;
	}


	_starpu_print_to_logfile("MODEL ERROR: OpenCL %d ERROR %lf EXEC %lf RATIO %lf NTASKS %d\n", args->devid, jobq->total_computation_time_error, jobq->total_computation_time, ratio, jobq->total_job_performed);
#endif

	pthread_exit(NULL);

	return NULL;
}

static unsigned _starpu_opencl_get_device_name(int dev, char *name, int lname)
{
	int err;

        if (!init_done) {
                _starpu_opencl_init();
        }

	// Get device name
	err = clGetDeviceInfo(devices[dev], CL_DEVICE_NAME, lname, name, NULL);
	if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

	_STARPU_OPENCL_DEBUG("Device %d : [%s]\n", dev, name);
	return EXIT_SUCCESS;
}

unsigned _starpu_opencl_get_device_count(void)
{
        if (!init_done) {
                _starpu_opencl_init();
        }
	return nb_devices;
}

static int _starpu_opencl_execute_job(starpu_job_t j, struct starpu_worker_s *args)
{
	int ret;
	uint32_t mask = 0;

	STARPU_ASSERT(j);
	struct starpu_task *task = j->task;

	struct timespec codelet_start, codelet_end;
	struct timespec codelet_start_comm, codelet_end_comm;

	unsigned calibrate_model = 0;
	int workerid = args->workerid;

	STARPU_ASSERT(task);
	struct starpu_codelet_t *cl = task->cl;
	STARPU_ASSERT(cl);

	if (cl->model && cl->model->benchmarking)
		calibrate_model = 1;

	/* we do not take communication into account when modeling the performance */
	if (STARPU_BENCHMARK_COMM)
	{
                //barrier(CLK_GLOBAL_MEM_FENCE);
		starpu_clock_gettime(&codelet_start_comm);
	}

	ret = _starpu_fetch_task_input(task, mask);
	if (ret != 0) {
		/* there was not enough memory, so the input of
		 * the codelet cannot be fetched ... put the
		 * codelet back, and try it later */
		return -EAGAIN;
	}

	if (STARPU_BENCHMARK_COMM)
	{
                //barrier(CLK_GLOBAL_MEM_FENCE);
		starpu_clock_gettime(&codelet_end_comm);
	}

	STARPU_TRACE_START_CODELET_BODY(j);

	struct starpu_task_profiling_info *profiling_info;
	profiling_info = task->profiling_info;

	if (profiling_info || calibrate_model || STARPU_BENCHMARK_COMM)
	{
		starpu_clock_gettime(&codelet_start);
		_starpu_worker_register_executing_start_date(workerid, &codelet_start);
	}

	args->status = STATUS_EXECUTING;
	task->status = STARPU_TASK_RUNNING;	

	cl_func func = cl->opencl_func;
	STARPU_ASSERT(func);
	func(task->interface, task->cl_arg);

	cl->per_worker_stats[workerid]++;

	if (profiling_info || calibrate_model || STARPU_BENCHMARK_COMM)
		starpu_clock_gettime(&codelet_end);

	STARPU_TRACE_END_CODELET_BODY(j);
	args->status = STATUS_UNKNOWN;

	_starpu_push_task_output(task, mask);

	_starpu_driver_update_job_feedback(j, args, profiling_info, calibrate_model,
			&codelet_start, &codelet_end, &codelet_start_comm, &codelet_end_comm);

	(void)STARPU_ATOMIC_ADD(&args->jobq->total_job_performed, 1);


	return EXIT_SUCCESS;
}
