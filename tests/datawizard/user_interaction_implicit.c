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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <starpu.h>
#include <stdlib.h>
#include <pthread.h>

#define NBUFFERS	16
#define NITER		128

struct data {
	unsigned index;
	unsigned val;
	starpu_data_handle handle;
};

struct data buffers[NBUFFERS];

void callback_sync_data(void *arg)
{
	struct data *data = arg;

	data->val++;

	starpu_data_release_from_mem(data->handle);
}

int main(int argc, char **argv)
{
	starpu_init(NULL);

	unsigned b;
	for (b = 0; b < NBUFFERS; b++)
	{
		buffers[b].index = b;
		starpu_variable_data_register(&buffers[b].handle, 0, (uintptr_t)&buffers[b].val, sizeof(unsigned));
	}

	unsigned iter;
	for (iter = 0; iter < NITER; iter++)
	for (b = 0; b < NBUFFERS; b++)
	{
		starpu_data_sync_with_mem_non_blocking(buffers[b].handle, STARPU_RW,
							callback_sync_data, &buffers[b]);
	}

	starpu_task_wait_for_all();

	/* do some cleanup */
	for (b = 0; b < NBUFFERS; b++)
	{
		starpu_data_unregister(buffers[b].handle);

		/* check result */
		if (buffers[b].val != NITER)
		{
			fprintf(stderr, "buffer[%d] = %d should be %d\n", b, buffers[b].val, NITER);
			STARPU_ABORT();
		}
	}

	starpu_shutdown();

	return 0;
}
