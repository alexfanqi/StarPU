/*
 * StarPU
 * Copyright (C) Université Bordeaux 1, CNRS 2009-2010 (see AUTHORS file)
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

#include <sys/time.h>
#include "stencil.h"

/* Main application */

/* default parameter values */
static unsigned  bind_tasks = 0;

static unsigned niter = 32;
static unsigned ticks = 1000;

#define SIZE 128

/* Problem size */
static unsigned sizex = SIZE;
static unsigned sizey = SIZE;
static unsigned sizez = 64*SIZE;

/* Number of blocks (scattered over the different MPI processes) */
unsigned nbz = 64;

/*
 *	Initialization
 */

unsigned get_bind_tasks(void)
{
	return bind_tasks;
}

unsigned get_nbz(void)
{
	return nbz;
}

unsigned get_niter(void)
{
	return niter;
}

unsigned get_ticks(void)
{
	return ticks;
}

static void parse_args(int argc, char **argv)
{
	int i;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-b") == 0) {
			bind_tasks = 1;
		}

		if (strcmp(argv[i], "-nbz") == 0) {
			nbz = atoi(argv[++i]);
		}

		if (strcmp(argv[i], "-sizex") == 0) {
			sizex = atoi(argv[++i]);
		}

		if (strcmp(argv[i], "-sizey") == 0) {
			sizey = atoi(argv[++i]);
		}

		if (strcmp(argv[i], "-sizez") == 0) {
			sizez = atoi(argv[++i]);
		}

		if (strcmp(argv[i], "-niter") == 0) {
			niter = atoi(argv[++i]);
		}

		if (strcmp(argv[i], "-ticks") == 0) {
			ticks = atoi(argv[++i]);
		}

		if (strcmp(argv[i], "-h") == 0) {
			 fprintf(stderr, "Usage : %s [options...]\n", argv[0]);
			 fprintf(stderr, "\n");
			 fprintf(stderr, "Options:\n");
			 fprintf(stderr, "-b			bind tasks on CPUs/GPUs\n");
			 fprintf(stderr, "-nbz <n>		Number of blocks on Z axis (%d by default)\n", nbz);
			 fprintf(stderr, "-size[xyz] <size>	Domain size on x/y/z axis (%dx%dx%d by default)\n", sizex, sizey, sizez);
			 fprintf(stderr, "-niter <n>		Number of iterations (%d by default)\n", niter);
			 fprintf(stderr, "-ticks <t>		How often to put ticks in the output (ms, %d by default)\n", ticks);
			 exit(0);
		}
	}
}

static void init_problem(int argc, char **argv, int rank, int world_size)
{
	parse_args(argc, argv);
	create_blocks_array(sizex, sizey, sizez, nbz);

	/* Select the MPI process which should compute the different blocks */
	assign_blocks_to_mpi_nodes(world_size);

	assign_blocks_to_workers(rank);

	/* Allocate the different memory blocks, if used by the MPI process */
	allocate_memory_on_node(rank);

	display_memory_consumption(rank);

	who_runs_what_len = 2*niter;
	who_runs_what = calloc(nbz * who_runs_what_len, sizeof(*who_runs_what));
	who_runs_what_index = calloc(nbz, sizeof(*who_runs_what_index));
	last_tick = calloc(nbz, sizeof(*last_tick));
}

/*
 *	Main body
 */

struct timeval start;
struct timeval end;
double timing; 

void f(unsigned task_per_worker[STARPU_NMAXWORKERS])
{
	unsigned total = 0;
	int worker;

	for (worker = 0; worker < STARPU_NMAXWORKERS; worker++)
		total += task_per_worker[worker];
	for (worker = 0; worker < STARPU_NMAXWORKERS; worker++) {
		if (task_per_worker[worker]) {
			char name[32];
			starpu_worker_get_name(worker, name, sizeof(name));
			fprintf(stderr,"\t%s -> %d (%2.2f%%)\n", name, task_per_worker[worker], (100.0*task_per_worker[worker])/total);
		}
	}
}

unsigned global_workerid(unsigned local_workerid)
{
#ifdef STARPU_USE_MPI
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	unsigned workers_per_node = starpu_worker_get_count();

	return (local_workerid + rank*workers_per_node);
#else
	return local_workerid;
#endif
}

int main(int argc, char **argv)
{
	int rank;
	int world_size;

#ifdef STARPU_USE_MPI
	int thread_support;
	if (MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &thread_support)) {
		fprintf(stderr, "MPI_Init_thread failed\n");
	}
	if (thread_support == MPI_THREAD_FUNNELED)
		fprintf(stderr,"Warning: MPI only has funneled thread support, not serialized, hoping this will work\n");
	if (thread_support < MPI_THREAD_FUNNELED)
		fprintf(stderr,"Warning: MPI does not have thread support!\n");
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
#else
	rank = 0;
	world_size = 1;
#endif

	if (rank == 0)
	{
		fprintf(stderr, "Running on %d nodes\n", world_size);
		fflush(stderr);
	}

	starpu_init(NULL);

#ifdef STARPU_USE_MPI
	starpu_mpi_initialize();
#endif

	init_problem(argc, argv, rank, world_size);

	create_tasks(rank);

#ifdef STARPU_USE_MPI
	int barrier_ret = MPI_Barrier(MPI_COMM_WORLD);
	STARPU_ASSERT(barrier_ret == MPI_SUCCESS);
#endif
	if (rank == 0)
		fprintf(stderr, "GO !\n");

	gettimeofday(&start, NULL);

	starpu_tag_notify_from_apps(TAG_INIT_TASK);

	wait_end_tasks(rank);

	gettimeofday(&end, NULL);

#ifdef STARPU_USE_MPI
	barrier_ret = MPI_Barrier(MPI_COMM_WORLD);
	STARPU_ASSERT(barrier_ret == MPI_SUCCESS);
#endif

#if 0
	check(rank);
#endif

	//display_debug(nbz, niter, rank);

#ifdef STARPU_USE_MPI
	starpu_mpi_shutdown();
#endif

	/* timing in us */
	timing = (double)((end.tv_sec - start.tv_sec)*1000000 + (end.tv_usec - start.tv_usec));

	double min_timing = timing;
	double max_timing = timing;
	double sum_timing = timing;

#ifdef STARPU_USE_MPI
	int reduce_ret;

	reduce_ret = MPI_Reduce(&timing, &min_timing, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
	STARPU_ASSERT(reduce_ret == MPI_SUCCESS);

	reduce_ret = MPI_Reduce(&timing, &max_timing, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
	STARPU_ASSERT(reduce_ret == MPI_SUCCESS);

	reduce_ret = MPI_Reduce(&timing, &sum_timing, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
	STARPU_ASSERT(reduce_ret == MPI_SUCCESS);

	/* XXX we should do a gather instead, here we assume that non initialized values are still 0 */
	int *who_runs_what_tmp = malloc(nbz * who_runs_what_len * sizeof(*who_runs_what));
	reduce_ret = MPI_Reduce(who_runs_what, who_runs_what_tmp, nbz * who_runs_what_len, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	STARPU_ASSERT(reduce_ret == MPI_SUCCESS);

	memcpy(who_runs_what, who_runs_what_tmp, nbz * who_runs_what_len * sizeof(*who_runs_what));

	/* XXX we should do a gather instead, here we assume that non initialized values are still 0 */
	int *who_runs_what_index_tmp = malloc(nbz * sizeof(*who_runs_what_index));
	reduce_ret = MPI_Reduce(who_runs_what_index, who_runs_what_index_tmp, nbz, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	STARPU_ASSERT(reduce_ret == MPI_SUCCESS);

	memcpy(who_runs_what_index, who_runs_what_index_tmp, nbz * sizeof(*who_runs_what_index));
#endif

	if (rank == 0)
	{
#if 1 
		fprintf(stderr, "update:\n");
		f(update_per_worker);
		fprintf(stderr, "top:\n");
		f(top_per_worker);
		fprintf(stderr, "bottom:\n");
		f(bottom_per_worker);
#endif
#if 1
		unsigned nzblocks_per_process = (nbz + world_size - 1) / world_size;

		unsigned bz, iter;
		unsigned last;
		for (iter = 0; iter < who_runs_what_len; iter++) {
			last = 1;
			for (bz = 0; bz < nbz; bz++) {
				if ((bz % nzblocks_per_process) == 0)
					fprintf(stderr, "| ");

				if (who_runs_what_index[bz] <= iter)
					fprintf(stderr,"_ ");
				else {
					last = 0;
					if (who_runs_what[bz + iter * nbz] == -1)
						fprintf(stderr,"* ");
					else
						fprintf(stderr, "%d ", who_runs_what[bz + iter * nbz]);
				}
			}
			fprintf(stderr, "\n");

			if (last)
				break;
		}
#endif

		fflush(stderr);

		fprintf(stdout, "Computation took: %lf ms on %d MPI processes\n", max_timing/1000, world_size);
		fprintf(stdout, "\tMIN : %lf ms\n", min_timing/1000);
		fprintf(stdout, "\tMAX : %lf ms\n", max_timing/1000);
		fprintf(stdout, "\tAVG : %lf ms\n", sum_timing/(world_size*1000));
	}

	starpu_shutdown();

#ifdef STARPU_USE_MPI
	MPI_Finalize();
#endif

	return 0;
}
