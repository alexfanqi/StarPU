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

#ifdef STARPU_USE_CUDA
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#endif
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <math.h>

#include <starpu.h>
#include <starpu_opencl.h>
#include <common/config.h>
#include <core/workers.h>
#include <core/perfmodel/perfmodel.h>

#define SIZE	(32*1024*1024*sizeof(char))
#define NITER	128

#define MAXCPUS	32

struct dev_timing {
	int cpu_id;
	double timing_htod;
	double timing_dtoh;
};

static double bandwidth_matrix[STARPU_MAXNODES][STARPU_MAXNODES] = {{-1.0}};
static double latency_matrix[STARPU_MAXNODES][STARPU_MAXNODES] = {{ -1.0}};
static unsigned was_benchmarked = 0;
static unsigned ncpus = 0;
static int ncuda = 0;
static int nopencl = 0;

/* Benchmarking the performance of the bus */

#ifdef STARPU_USE_CUDA
static int cuda_affinity_matrix[STARPU_MAXCUDADEVS][MAXCPUS];
static double cudadev_timing_htod[STARPU_MAXNODES] = {0.0};
static double cudadev_timing_dtoh[STARPU_MAXNODES] = {0.0};
static struct dev_timing cudadev_timing_per_cpu[STARPU_MAXNODES*MAXCPUS];
#endif
#ifdef STARPU_USE_OPENCL
static int opencl_affinity_matrix[STARPU_MAXOPENCLDEVS][MAXCPUS];
static double opencldev_timing_htod[STARPU_MAXNODES] = {0.0};
static double opencldev_timing_dtoh[STARPU_MAXNODES] = {0.0};
static struct dev_timing opencldev_timing_per_cpu[STARPU_MAXNODES*MAXCPUS];
#endif

#if defined(STARPU_USE_CUDA) || defined(STARPU_USE_OPENCL)

#ifdef STARPU_HAVE_HWLOC
static hwloc_topology_t hwtopology;
#endif

#ifdef STARPU_USE_CUDA
static void measure_bandwidth_between_host_and_dev_on_cpu_with_cuda(int dev, int cpu, struct dev_timing *dev_timing_per_cpu)
{
	struct starpu_machine_config_s *config = _starpu_get_machine_config();
	_starpu_bind_thread_on_cpu(config, cpu);

	/* Initiliaze CUDA context on the device */
	cudaSetDevice(dev);

	/* hack to avoid third party libs to rebind threads */
	_starpu_bind_thread_on_cpu(config, cpu);

	/* hack to force the initialization */
	cudaFree(0);

	/* hack to avoid third party libs to rebind threads */
	_starpu_bind_thread_on_cpu(config, cpu);


	/* Allocate a buffer on the device */
	unsigned char *d_buffer;
	cudaMalloc((void **)&d_buffer, SIZE);
	assert(d_buffer);

	/* hack to avoid third party libs to rebind threads */
	_starpu_bind_thread_on_cpu(config, cpu);


	/* Allocate a buffer on the host */
	unsigned char *h_buffer;
	cudaHostAlloc((void **)&h_buffer, SIZE, 0);
	assert(h_buffer);

	/* hack to avoid third party libs to rebind threads */
	_starpu_bind_thread_on_cpu(config, cpu);


	/* Fill them */
	memset(h_buffer, 0, SIZE);
	cudaMemset(d_buffer, 0, SIZE);

	/* hack to avoid third party libs to rebind threads */
	_starpu_bind_thread_on_cpu(config, cpu);


	unsigned iter;
	double timing;
	struct timeval start;
	struct timeval end;

	/* Measure upload bandwidth */
	gettimeofday(&start, NULL);
	for (iter = 0; iter < NITER; iter++)
	{
		cudaMemcpy(d_buffer, h_buffer, SIZE, cudaMemcpyHostToDevice);
		cudaThreadSynchronize();
	}
	gettimeofday(&end, NULL);
	timing = (double)((end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec));

	dev_timing_per_cpu[(dev+1)*MAXCPUS+cpu].timing_htod = timing/NITER;

	/* Measure download bandwidth */
	gettimeofday(&start, NULL);
	for (iter = 0; iter < NITER; iter++)
	{
		cudaMemcpy(h_buffer, d_buffer, SIZE, cudaMemcpyDeviceToHost);
		cudaThreadSynchronize();
	}
	gettimeofday(&end, NULL);
	timing = (double)((end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec));

	dev_timing_per_cpu[(dev+1)*MAXCPUS+cpu].timing_dtoh = timing/NITER;

	/* Free buffers */
	cudaFreeHost(h_buffer);
	cudaFree(d_buffer);

	cudaThreadExit();
}
#endif

#ifdef STARPU_USE_OPENCL
static void measure_bandwidth_between_host_and_dev_on_cpu_with_opencl(int dev, int cpu, struct dev_timing *dev_timing_per_cpu)
{
        cl_context context;
        cl_command_queue queue;

        struct starpu_machine_config_s *config = _starpu_get_machine_config();
	_starpu_bind_thread_on_cpu(config, cpu);

	/* Initialize OpenCL context on the device */
        _starpu_opencl_init_context(dev);
        starpu_opencl_get_context(dev, &context);
        starpu_opencl_get_queue(dev, &queue);

	/* hack to avoid third party libs to rebind threads */
	_starpu_bind_thread_on_cpu(config, cpu);

	/* Allocate a buffer on the device */
        int err;
	cl_mem d_buffer;
	d_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, SIZE, NULL, &err);
	if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

	/* hack to avoid third party libs to rebind threads */
	_starpu_bind_thread_on_cpu(config, cpu);

        /* Allocate a buffer on the host */
	unsigned char *h_buffer;
        h_buffer = malloc(SIZE);
	assert(h_buffer);

	/* hack to avoid third party libs to rebind threads */
	_starpu_bind_thread_on_cpu(config, cpu);

        /* Fill them */
	memset(h_buffer, 0, SIZE);
        err = clEnqueueWriteBuffer(queue, d_buffer, CL_TRUE, 0, SIZE, h_buffer, 0, NULL, NULL);
        if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

	/* hack to avoid third party libs to rebind threads */
	_starpu_bind_thread_on_cpu(config, cpu);

        unsigned iter;
	double timing;
	struct timeval start;
	struct timeval end;

	/* Measure upload bandwidth */
	gettimeofday(&start, NULL);
	for (iter = 0; iter < NITER; iter++)
	{
                err = clEnqueueWriteBuffer(queue, d_buffer, CL_TRUE, 0, SIZE, h_buffer, 0, NULL, NULL);
                if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);
	}
	gettimeofday(&end, NULL);
	timing = (double)((end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec));

	dev_timing_per_cpu[(dev+1)*MAXCPUS+cpu].timing_htod = timing/NITER;

	/* Measure download bandwidth */
	gettimeofday(&start, NULL);
	for (iter = 0; iter < NITER; iter++)
	{
                err = clEnqueueReadBuffer(queue, d_buffer, CL_TRUE, 0, SIZE, h_buffer, 0, NULL, NULL);
                if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);
	}
	gettimeofday(&end, NULL);
	timing = (double)((end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec));

	dev_timing_per_cpu[(dev+1)*MAXCPUS+cpu].timing_dtoh = timing/NITER;

	/* Free buffers */
	clReleaseMemObject(d_buffer);
	free(h_buffer);

	/* Uninitiliaze OpenCL context on the device */
        _starpu_opencl_deinit_context(dev);
}
#endif

/* NB: we want to sort the bandwidth by DECREASING order */
static int compar_dev_timing(const void *left_dev_timing, const void *right_dev_timing)
{
	const struct dev_timing *left = left_dev_timing;
	const struct dev_timing *right = right_dev_timing;

	double left_dtoh = left->timing_dtoh;
	double left_htod = left->timing_htod;
	double right_dtoh = right->timing_dtoh;
	double right_htod = right->timing_htod;

	double bandwidth_sum2_left = left_dtoh*left_dtoh + left_htod*left_htod;
	double bandwidth_sum2_right = right_dtoh*right_dtoh + right_htod*right_htod;

	/* it's for a decreasing sorting */
	return (bandwidth_sum2_left < bandwidth_sum2_right);
}

#ifdef STARPU_HAVE_HWLOC
static int find_numa_node(hwloc_obj_t obj)
{
	STARPU_ASSERT(obj);
	hwloc_obj_t current = obj;

	while (current->depth != HWLOC_OBJ_NODE)
	{
		current = current->parent;

		/* If we don't find a "node" obj before the root, this means
		 * hwloc does not know whether there are numa nodes or not, so
		 * we should not use a per-node sampling in that case. */
		STARPU_ASSERT(current);
	}

	STARPU_ASSERT(current->depth == HWLOC_OBJ_NODE);

	return current->logical_index; 
}
#endif

static void measure_bandwidth_between_cpus_and_dev(int dev, struct dev_timing *dev_timing_per_cpu, char type)
{
	/* Either we have hwloc and we measure the bandwith between each GPU
	 * and each NUMA node, or we don't have such NUMA information and we
	 * measure the bandwith for each pair of (CPU, GPU), which is slower.
	 * */
#ifdef STARPU_HAVE_HWLOC
	int cpu_depth = hwloc_get_type_depth(hwtopology, HWLOC_OBJ_CORE);
	int nnuma_nodes = hwloc_get_nbobjs_by_depth(hwtopology, HWLOC_OBJ_NODE);

	/* If no NUMA node was found, we assume that we have a single memory
	 * bank. */
	const unsigned no_node_obj_was_found = (nnuma_nodes == 0);
	
	unsigned is_available_per_numa_node[nnuma_nodes];
	double dev_timing_htod_per_numa_node[nnuma_nodes];
	double dev_timing_dtoh_per_numa_node[nnuma_nodes];

	memset(is_available_per_numa_node, 0, nnuma_nodes*sizeof(unsigned));
#endif

	unsigned cpu;
	for (cpu = 0; cpu < ncpus; cpu++)
	{
		dev_timing_per_cpu[(dev+1)*MAXCPUS+cpu].cpu_id = cpu;

#ifdef STARPU_HAVE_HWLOC
		int numa_id = 0;

		if (!no_node_obj_was_found)
		{
			hwloc_obj_t obj = hwloc_get_obj_by_depth(hwtopology, cpu_depth, cpu);
	
			numa_id = find_numa_node(obj);
	
			if (is_available_per_numa_node[numa_id])
			{
				/* We reuse the previous numbers for that NUMA node */
				dev_timing_per_cpu[(dev+1)*MAXCPUS+cpu].timing_htod =
					dev_timing_htod_per_numa_node[numa_id];
				dev_timing_per_cpu[(dev+1)*MAXCPUS+cpu].timing_dtoh =
					dev_timing_dtoh_per_numa_node[numa_id];
				continue;
			}
		}
#endif

#ifdef STARPU_USE_CUDA
                if (type == 'C')
                        measure_bandwidth_between_host_and_dev_on_cpu_with_cuda(dev, cpu, dev_timing_per_cpu);
#endif
#ifdef STARPU_USE_OPENCL
                if (type == 'O')
                        measure_bandwidth_between_host_and_dev_on_cpu_with_opencl(dev, cpu, dev_timing_per_cpu);
#endif

#ifdef STARPU_HAVE_HWLOC
		if (!no_node_obj_was_found && !is_available_per_numa_node[numa_id])
		{
			/* Save the results for that NUMA node */
			dev_timing_htod_per_numa_node[numa_id] =
				dev_timing_per_cpu[(dev+1)*MAXCPUS+cpu].timing_htod;
			dev_timing_dtoh_per_numa_node[numa_id] =
				dev_timing_per_cpu[(dev+1)*MAXCPUS+cpu].timing_dtoh;

			is_available_per_numa_node[numa_id] = 1;
		}
#endif
        }
}

static void measure_bandwidth_between_host_and_dev(int dev, double *dev_timing_htod, double *dev_timing_dtoh,
                                                   struct dev_timing *dev_timing_per_cpu, char type)
{
	measure_bandwidth_between_cpus_and_dev(dev, dev_timing_per_cpu, type);

	/* sort the results */
	qsort(&(dev_timing_per_cpu[(dev+1)*MAXCPUS]), ncpus,
              sizeof(struct dev_timing),
			compar_dev_timing);

#ifdef STARPU_VERBOSE
        unsigned cpu;
	for (cpu = 0; cpu < ncpus; cpu++)
	{
		unsigned current_cpu = dev_timing_per_cpu[(dev+1)*MAXCPUS+cpu].cpu_id;
		double bandwidth_dtoh = dev_timing_per_cpu[(dev+1)*MAXCPUS+cpu].timing_dtoh;
		double bandwidth_htod = dev_timing_per_cpu[(dev+1)*MAXCPUS+cpu].timing_htod;

		double bandwidth_sum2 = bandwidth_dtoh*bandwidth_dtoh + bandwidth_htod*bandwidth_htod;

		fprintf(stderr, "BANDWIDTH GPU %d CPU %d - htod %lf - dtoh %lf - %lf\n", dev, current_cpu, bandwidth_htod, bandwidth_dtoh, sqrt(bandwidth_sum2));
	}

	unsigned best_cpu = dev_timing_per_cpu[(dev+1)*MAXCPUS+0].cpu_id;

	fprintf(stderr, "BANDWIDTH GPU %d BEST CPU %d\n", dev, best_cpu);
#endif

	/* The results are sorted in a decreasing order, so that the best
	 * measurement is currently the first entry. */
	dev_timing_dtoh[dev+1] = dev_timing_per_cpu[(dev+1)*MAXCPUS+0].timing_dtoh;
	dev_timing_htod[dev+1] = dev_timing_per_cpu[(dev+1)*MAXCPUS+0].timing_htod;
}
#endif /* defined(STARPU_USE_CUDA) || defined(STARPU_USE_OPENCL) */

static void benchmark_all_gpu_devices(void)
{
#if defined(STARPU_USE_CUDA) || defined(STARPU_USE_OPENCL)
	int i, ret;

#ifdef STARPU_VERBOSE
	fprintf(stderr, "Benchmarking the speed of the bus\n");
#endif

#ifdef STARPU_HAVE_HWLOC
	hwloc_topology_init(&hwtopology);
	hwloc_topology_load(hwtopology);
#endif

	/* TODO: use hwloc */
#ifndef __MINGW32__
	/* Save the current cpu binding */
	cpu_set_t former_process_affinity;
	ret = sched_getaffinity(0, sizeof(former_process_affinity), &former_process_affinity);
	if (ret)
	{
		perror("sched_getaffinity");
		STARPU_ABORT();
	}
#endif

	struct starpu_machine_config_s *config = _starpu_get_machine_config();
	ncpus = _starpu_topology_get_nhwcpu(config);

#ifdef STARPU_USE_CUDA
        cudaGetDeviceCount(&ncuda);
	for (i = 0; i < ncuda; i++)
	{
		/* measure bandwidth between Host and Device i */
		measure_bandwidth_between_host_and_dev(i, cudadev_timing_htod, cudadev_timing_dtoh, cudadev_timing_per_cpu, 'C');
	}
#endif
#ifdef STARPU_USE_OPENCL
        nopencl = _starpu_opencl_get_device_count();
	for (i = 0; i < nopencl; i++)
	{
		/* measure bandwith between Host and Device i */
		measure_bandwidth_between_host_and_dev(i, opencldev_timing_htod, opencldev_timing_dtoh, opencldev_timing_per_cpu, 'O');
	}
#endif

	/* FIXME: use hwloc */
#ifndef __MINGW32__
	/* Restore the former affinity */
	ret = sched_setaffinity(0, sizeof(former_process_affinity), &former_process_affinity);
	if (ret)
	{
		perror("sched_setaffinity");
		STARPU_ABORT();
	}
#endif

#ifdef STARPU_HAVE_HWLOC
	hwloc_topology_destroy(hwtopology);
#endif

#ifdef STARPU_VERBOSE
	fprintf(stderr, "Benchmarking the speed of the bus is done.\n");
#endif
#endif /* defined(STARPU_USE_CUDA) || defined(STARPU_USE_OPENCL) */

	was_benchmarked = 1;
}

static void get_bus_path(const char *type, char *path, size_t maxlen)
{
	_starpu_get_perf_model_dir_bus(path, maxlen);
	strncat(path, type, maxlen);

	char hostname[32];
	gethostname(hostname, 32);
	strncat(path, ".", maxlen);
	strncat(path, hostname, maxlen);
}

/*
 *	Affinity
 */

static void get_affinity_path(char *path, size_t maxlen)
{
	get_bus_path("affinity", path, maxlen);
}

static void load_bus_affinity_file_content(void)
{
	FILE *f;

	char path[256];
	get_affinity_path(path, 256);

	f = fopen(path, "r");
	STARPU_ASSERT(f);

#if defined(STARPU_USE_CUDA) || defined(STARPU_USE_OPENCL)
	struct starpu_machine_config_s *config = _starpu_get_machine_config();
	ncpus = _starpu_topology_get_nhwcpu(config);
        int gpu;

#ifdef STARPU_USE_CUDA
        cudaGetDeviceCount(&ncuda);
	for (gpu = 0; gpu < ncuda; gpu++)
	{
		int ret;

		int dummy;

		_starpu_drop_comments(f);
		ret = fscanf(f, "%d\t", &dummy);
		STARPU_ASSERT(ret == 1);

		STARPU_ASSERT(dummy == gpu);

		unsigned cpu;
		for (cpu = 0; cpu < ncpus; cpu++)
		{
			ret = fscanf(f, "%d\t", &cuda_affinity_matrix[gpu][cpu]);
			STARPU_ASSERT(ret == 1);
		}

		ret = fscanf(f, "\n");
		STARPU_ASSERT(ret == 0);
	}
#endif
#ifdef STARPU_USE_OPENCL
        nopencl = _starpu_opencl_get_device_count();
	for (gpu = 0; gpu < nopencl; gpu++)
	{
		int ret;

		int dummy;

		_starpu_drop_comments(f);
		ret = fscanf(f, "%d\t", &dummy);
		STARPU_ASSERT(ret == 1);

		STARPU_ASSERT(dummy == gpu);

		unsigned cpu;
		for (cpu = 0; cpu < ncpus; cpu++)
		{
			ret = fscanf(f, "%d\t", &opencl_affinity_matrix[gpu][cpu]);
			STARPU_ASSERT(ret == 1);
		}

		ret = fscanf(f, "\n");
		STARPU_ASSERT(ret == 0);
	}
#endif
#endif

	fclose(f);
}

static void write_bus_affinity_file_content(void)
{
	FILE *f;

	STARPU_ASSERT(was_benchmarked);

	char path[256];
	get_affinity_path(path, 256);

	f = fopen(path, "w+");
	if (!f)
	{
		perror("fopen write_buf_affinity_file_content");
		fprintf(stderr,"path '%s'\n", path);
		fflush(stderr);
		STARPU_ABORT();
	}

#if defined(STARPU_USE_CUDA) || defined(STARPU_USE_OPENCL)
	unsigned cpu;
        int gpu;

        fprintf(f, "# GPU\t");
	for (cpu = 0; cpu < ncpus; cpu++)
		fprintf(f, "CPU%d\t", cpu);
	fprintf(f, "\n");

#ifdef STARPU_USE_CUDA
	for (gpu = 0; gpu < ncuda; gpu++)
	{
		fprintf(f, "%d\t", gpu);

		for (cpu = 0; cpu < ncpus; cpu++)
		{
			fprintf(f, "%d\t", cudadev_timing_per_cpu[(gpu+1)*MAXCPUS+cpu].cpu_id);
		}

		fprintf(f, "\n");
	}
#endif
#ifdef STARPU_USE_OPENCL
	for (gpu = 0; gpu < nopencl; gpu++)
	{
		fprintf(f, "%d\t", gpu);

		for (cpu = 0; cpu < ncpus; cpu++)
		{
                        fprintf(f, "%d\t", opencldev_timing_per_cpu[(gpu+1)*MAXCPUS+cpu].cpu_id);
		}

		fprintf(f, "\n");
	}
#endif

	fclose(f);
#endif
}

static void generate_bus_affinity_file(void)
{
	if (!was_benchmarked)
		benchmark_all_gpu_devices();

	write_bus_affinity_file_content();
}

static void load_bus_affinity_file(void)
{
	int res;

	char path[256];
	get_affinity_path(path, 256);

	res = access(path, F_OK);
	if (res)
	{
		/* File does not exist yet */
		generate_bus_affinity_file();
	}

	load_bus_affinity_file_content();
}

#ifdef STARPU_USE_CUDA
int *_starpu_get_cuda_affinity_vector(unsigned gpuid)
{
        return cuda_affinity_matrix[gpuid];
}
#endif /* STARPU_USE_CUDA */

#ifdef STARPU_USE_OPENCL
int *_starpu_get_opencl_affinity_vector(unsigned gpuid)
{
        return opencl_affinity_matrix[gpuid];
}
#endif /* STARPU_USE_OPENCL */

/*
 *	Latency
 */

static void get_latency_path(char *path, size_t maxlen)
{
	get_bus_path("latency", path, maxlen);
}

static void load_bus_latency_file_content(void)
{
	int n;
	unsigned src, dst;
	FILE *f;

	char path[256];
	get_latency_path(path, 256);

	f = fopen(path, "r");
	STARPU_ASSERT(f);

	for (src = 0; src < STARPU_MAXNODES; src++)
	{
		_starpu_drop_comments(f);
		for (dst = 0; dst < STARPU_MAXNODES; dst++)
		{
			double latency;

			n = fscanf(f, "%lf\t", &latency);
			STARPU_ASSERT(n == 1);

			latency_matrix[src][dst] = latency;
		}

		n = fscanf(f, "\n");
		STARPU_ASSERT(n == 0);
	}

	fclose(f);
}

static void write_bus_latency_file_content(void)
{
        int src, dst, maxnode;
	FILE *f;

	STARPU_ASSERT(was_benchmarked);

	char path[256];
	get_latency_path(path, 256);

	f = fopen(path, "w+");
	if (!f)
	{
		perror("fopen write_bus_latency_file_content");
		fprintf(stderr,"path '%s'\n", path);
		fflush(stderr);
		STARPU_ABORT();
	}

	fprintf(f, "# ");
	for (dst = 0; dst < STARPU_MAXNODES; dst++)
		fprintf(f, "to %d\t\t", dst);
	fprintf(f, "\n");

        maxnode = ncuda;
#ifdef STARPU_USE_OPENCL
        maxnode += nopencl;
#endif
        for (src = 0; src < STARPU_MAXNODES; src++)
	{
		for (dst = 0; dst < STARPU_MAXNODES; dst++)
		{
			double latency;

			if ((src > maxnode) || (dst > maxnode))
			{
				/* convention */
				latency = -1.0;
			}
			else if (src == dst)
			{
				latency = 0.0;
			}
			else {
                                latency = ((src && dst)?2000.0:500.0);
			}

			fprintf(f, "%lf\t", latency);
		}

		fprintf(f, "\n");
	}

	fclose(f);
}

static void generate_bus_latency_file(void)
{
	if (!was_benchmarked)
		benchmark_all_gpu_devices();

	write_bus_latency_file_content();
}

static void load_bus_latency_file(void)
{
	int res;

	char path[256];
	get_latency_path(path, 256);

	res = access(path, F_OK);
	if (res)
	{
		/* File does not exist yet */
		generate_bus_latency_file();
	}

	load_bus_latency_file_content();
}


/*
 *	Bandwidth
 */
static void get_bandwidth_path(char *path, size_t maxlen)
{
	get_bus_path("bandwidth", path, maxlen);
}

static void load_bus_bandwidth_file_content(void)
{
	int n;
	unsigned src, dst;
	FILE *f;

	char path[256];
	get_bandwidth_path(path, 256);

	f = fopen(path, "r");
	if (!f)
	{
		perror("fopen load_bus_bandwidth_file_content");
		fprintf(stderr,"path '%s'\n", path);
		fflush(stderr);
		STARPU_ABORT();
	}

	for (src = 0; src < STARPU_MAXNODES; src++)
	{
		_starpu_drop_comments(f);
		for (dst = 0; dst < STARPU_MAXNODES; dst++)
		{
			double bandwidth;

			n = fscanf(f, "%lf\t", &bandwidth);
			STARPU_ASSERT(n == 1);

			bandwidth_matrix[src][dst] = bandwidth;
		}

		n = fscanf(f, "\n");
		STARPU_ASSERT(n == 0);
	}

	fclose(f);
}

static void write_bus_bandwidth_file_content(void)
{
	int src, dst, maxnode;
	FILE *f;

	STARPU_ASSERT(was_benchmarked);

	char path[256];
	get_bandwidth_path(path, 256);

	f = fopen(path, "w+");
	STARPU_ASSERT(f);

	fprintf(f, "# ");
	for (dst = 0; dst < STARPU_MAXNODES; dst++)
		fprintf(f, "to %d\t\t", dst);
	fprintf(f, "\n");

        maxnode = ncuda;
#ifdef STARPU_USE_OPENCL
        maxnode += nopencl;
#endif
	for (src = 0; src < STARPU_MAXNODES; src++)
	{
		for (dst = 0; dst < STARPU_MAXNODES; dst++)
		{
			double bandwidth;

			if ((src > maxnode) || (dst > maxnode))
			{
				bandwidth = -1.0;
			}
#if defined(STARPU_USE_CUDA) || defined(STARPU_USE_OPENCL)
			else if (src != dst)
			{
                                double time_src_to_ram=0.0, time_ram_to_dst=0.0;
                                /* Bandwidth = (SIZE)/(time i -> ram + time ram -> j)*/
#ifdef STARPU_USE_CUDA
				time_src_to_ram = (src==0)?0.0:cudadev_timing_dtoh[src];
                                time_ram_to_dst = (dst==0)?0.0:cudadev_timing_htod[dst];
#endif
#ifdef STARPU_USE_OPENCL
                                if (src > ncuda)
                                        time_src_to_ram = (src==0)?0.0:opencldev_timing_dtoh[src-ncuda];
                                if (dst > ncuda)
                                        time_ram_to_dst = (dst==0)?0.0:opencldev_timing_htod[dst-ncuda];
#endif

				double timing =time_src_to_ram + time_ram_to_dst;

				bandwidth = 1.0*SIZE/timing;
			}
#endif
			else {
			        /* convention */
			        bandwidth = 0.0;
			}

			fprintf(f, "%lf\t", bandwidth);
		}

		fprintf(f, "\n");
	}

	fclose(f);
}

static void generate_bus_bandwidth_file(void)
{
	if (!was_benchmarked)
		benchmark_all_gpu_devices();

	write_bus_bandwidth_file_content();
}

static void load_bus_bandwidth_file(void)
{
	int res;

	char path[256];
	get_bandwidth_path(path, 256);

	res = access(path, F_OK);
	if (res)
	{
		/* File does not exist yet */
		generate_bus_bandwidth_file();
	}

	load_bus_bandwidth_file_content();
}

/*
 *	Config
 */
static void get_config_path(char *path, size_t maxlen)
{
	get_bus_path("config", path, maxlen);
}

static void check_bus_config_file()
{
        int res;
        char path[256];

        get_config_path(path, 256);
        res = access(path, F_OK);
        if (res) {
		fprintf(stderr, "No performance model for the bus, calibrating...");
		starpu_force_bus_sampling();
		fprintf(stderr, "done\n");
        }
        else {
                FILE *f;
                int ret, read_cuda, read_opencl;
                unsigned read_cpus;
                struct starpu_machine_config_s *config = _starpu_get_machine_config();

                // Loading configuration from file
                f = fopen(path, "r");
                STARPU_ASSERT(f);
                _starpu_drop_comments(f);
                ret = fscanf(f, "%d\t", &read_cpus);
		STARPU_ASSERT(ret == 1);
                _starpu_drop_comments(f);
		ret = fscanf(f, "%d\t", &read_cuda);
		STARPU_ASSERT(ret == 1);
                _starpu_drop_comments(f);
		ret = fscanf(f, "%d\t", &read_opencl);
		STARPU_ASSERT(ret == 1);
                _starpu_drop_comments(f);
                fclose(f);

                // Loading current configuration
                ncpus = _starpu_topology_get_nhwcpu(config);
#ifdef STARPU_USE_CUDA
                cudaGetDeviceCount(&ncuda);
#endif
#ifdef STARPU_USE_OPENCL
                nopencl = _starpu_opencl_get_device_count();
#endif

                // Checking if both configurations match
                if (read_cpus != ncpus) {
			fprintf(stderr, "Current configuration does not match the performance model (CPUS: %d != %d), recalibrating...", read_cpus, ncpus);
                        starpu_force_bus_sampling();
			fprintf(stderr, "done\n");
                }
                else if (read_cuda != ncuda) {
                        fprintf(stderr, "Current configuration does not match the performance model (CUDA: %d != %d), recalibrating...", read_cuda, ncuda);
                        starpu_force_bus_sampling();
			fprintf(stderr, "done\n");
                }
                else if (read_opencl != nopencl) {
                        fprintf(stderr, "Current configuration does not match the performance model (OpenCL: %d != %d), recalibrating...", read_opencl, nopencl);
                        starpu_force_bus_sampling();
			fprintf(stderr, "done\n");
                }
        }
}

static void write_bus_config_file_content(void)
{
	FILE *f;
	char path[256];

	STARPU_ASSERT(was_benchmarked);
        get_config_path(path, 256);
        f = fopen(path, "w+");
	STARPU_ASSERT(f);

        fprintf(f, "# Current configuration\n");
        fprintf(f, "%d # Number of CPUs\n", ncpus);
        fprintf(f, "%d # Number of CUDA devices\n", ncuda);
        fprintf(f, "%d # Number of OpenCL devices\n", nopencl);

        fclose(f);
}

static void generate_bus_config_file()
{
	if (!was_benchmarked)
		benchmark_all_gpu_devices();

	write_bus_config_file_content();
}

/*
 *	Generic
 */

void starpu_force_bus_sampling(void)
{
	_starpu_create_sampling_directory_if_needed();

	generate_bus_affinity_file();
	generate_bus_latency_file();
	generate_bus_bandwidth_file();
        generate_bus_config_file();
}

void _starpu_load_bus_performance_files(void)
{
	_starpu_create_sampling_directory_if_needed();

        check_bus_config_file();
	load_bus_affinity_file();
	load_bus_latency_file();
	load_bus_bandwidth_file();
}

double _starpu_predict_transfer_time(unsigned src_node, unsigned dst_node, size_t size)
{
	double bandwidth = bandwidth_matrix[src_node][dst_node];
	double latency = latency_matrix[src_node][dst_node];

	return latency + size/bandwidth;
}
