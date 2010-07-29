/*
 * StarPU
 * Copyright (C) Université Bordeaux 1, CNRS 2008-2009 (see AUTHORS file)
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

#include <starpu_mpi.h>
#include <stdlib.h>

#define NITER	2048

#define BIGSIZE	128
#define SIZE	64

int main(int argc, char **argv)
{
	MPI_Init(NULL, NULL);

	int rank, size;

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	if (size < 2)
	{
		if (rank == 0)
			fprintf(stderr, "We need at least processes.\n");

		MPI_Finalize();
		return 0;
	}

	/* We only use 2 nodes for that test */
	if (rank >= 2)
	{
		MPI_Finalize();
		return 0;
	}
		
	starpu_init(NULL);
	starpu_mpi_initialize();

	/* Node 0 will allocate a big block and only register an inner part of
	 * it as the block data, Node 1 will allocate a block of small size and
	 * register it directly. Node 0 and 1 will then exchange the content of
	 * their blocks. */

	float *block;
	starpu_data_handle block_handle;

	if (rank == 0)
	{
		block = calloc(BIGSIZE*BIGSIZE*BIGSIZE, sizeof(float));
		assert(block);

		/* fill the inner block */
		unsigned i, j, k;
		for (k = 0; k < SIZE; k++)
		for (j = 0; j < SIZE; j++)
		for (i = 0; i < SIZE; i++)
		{
			block[i + j*BIGSIZE + k*BIGSIZE*BIGSIZE] = 1.0f;
		}

		starpu_block_data_register(&block_handle, 0,
			(uintptr_t)block, BIGSIZE, BIGSIZE*BIGSIZE,
			SIZE, SIZE, SIZE, sizeof(float));
	}
	else /* rank == 1 */
	{
		block = calloc(SIZE*SIZE*SIZE, sizeof(float));
		assert(block);

		starpu_block_data_register(&block_handle, 0,
			(uintptr_t)block, SIZE, SIZE*SIZE,
			SIZE, SIZE, SIZE, sizeof(float));
	}

	if (rank == 0)
	{
		starpu_mpi_send(block_handle, 1, 0x42, MPI_COMM_WORLD);

		MPI_Status status;
		starpu_mpi_recv(block_handle, 1, 0x1337, MPI_COMM_WORLD, &status);

		/* check the content of the block */
		starpu_data_acquire(block_handle, STARPU_R);
		unsigned i, j, k;
		for (k = 0; k < SIZE; k++)
		for (j = 0; j < SIZE; j++)
		for (i = 0; i < SIZE; i++)
		{
			assert(block[i + j*BIGSIZE + k*BIGSIZE*BIGSIZE] == 33.0f);
		}
		starpu_data_release(block_handle);
		
	}
	else /* rank == 1 */
	{
		MPI_Status status;
		starpu_mpi_recv(block_handle, 0, 0x42, MPI_COMM_WORLD, &status);

		/* check the content of the block and modify it */
		starpu_data_acquire(block_handle, STARPU_RW);
		unsigned i, j, k;
		for (k = 0; k < SIZE; k++)
		for (j = 0; j < SIZE; j++)
		for (i = 0; i < SIZE; i++)
		{
			assert(block[i + j*SIZE + k*SIZE*SIZE] == 1.0f);
			block[i + j*SIZE + k*SIZE*SIZE] = 33.0f;
		}
		starpu_data_release(block_handle);

		starpu_mpi_send(block_handle, 0, 0x1337, MPI_COMM_WORLD);
	}

	fprintf(stdout, "Rank %d is done\n", rank);
	fflush(stdout);

	starpu_mpi_shutdown();
	starpu_shutdown();

	MPI_Finalize();

	return 0;
}
