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
#include <common/config.h>
#include <datawizard/filters.h>

/*
 * an example of a dummy partition function : blocks ...
 */
void starpu_block_filter_func(starpu_filter *f, starpu_data_handle root_handle)
{
	unsigned nchunks;
	uint32_t arg = f->filter_arg;

	starpu_matrix_interface_t *matrix_root =
		starpu_data_get_interface_on_node(root_handle, 0);

	uint32_t nx = matrix_root->nx;
	uint32_t ny = matrix_root->ny;
	size_t elemsize = matrix_root->elemsize;

	/* we will have arg chunks */
	nchunks = STARPU_MIN(nx, arg);

	/* first allocate the children, they have the same interface type as
	 * the root (matrix) */
	starpu_data_create_children(root_handle, nchunks, root_handle->ops);

	/* actually create all the chunks */
	unsigned chunk;
	for (chunk = 0; chunk < nchunks; chunk++)
	{
		size_t chunk_size = ((size_t)nx + nchunks - 1)/nchunks;
		size_t offset = (size_t)chunk*chunk_size*elemsize;

		uint32_t child_nx = 
			STARPU_MIN(chunk_size, (size_t)nx - (size_t)chunk*chunk_size);

		starpu_data_handle chunk_handle =
			starpu_data_get_child(root_handle, chunk);

		starpu_memory_node node;
		for (node = 0; node < starpu_memory_nodes_count(); node++)
		{
			starpu_matrix_interface_t *local = 
				starpu_data_get_interface_on_node(chunk_handle, node);

			local->nx = child_nx;
			local->ny = ny;
			local->elemsize = elemsize;

			if (starpu_data_test_if_allocated_on_node(root_handle, node)) {
				starpu_matrix_interface_t *local_root =
					starpu_data_get_interface_on_node(root_handle, node);

				local->ptr = local_root->ptr + offset;
				local->ld = local_root->ld;
                                local->dev_handle = local_root->dev_handle;
                                local->offset = local_root->offset + offset;
			}
		}
	}
}

void starpu_vertical_block_filter_func(starpu_filter *f, starpu_data_handle root_handle)
{
	unsigned nchunks;
	uint32_t arg = f->filter_arg;

	starpu_matrix_interface_t *interface =
		starpu_data_get_interface_on_node(root_handle, 0);

	uint32_t nx = interface->nx;
	uint32_t ny = interface->ny;
	size_t elemsize = interface->elemsize;

	/* we will have arg chunks */
	nchunks = STARPU_MIN(ny, arg);
	
	/* first allocate the children: they also use a BLAS interface */
	starpu_data_create_children(root_handle, nchunks, root_handle->ops);

	/* actually create all the chunks */
	unsigned chunk;
	for (chunk = 0; chunk < nchunks; chunk++)
	{
		size_t chunk_size = ((size_t)ny + nchunks - 1)/nchunks;

		size_t child_ny = 
			STARPU_MIN(chunk_size, (size_t)ny - (size_t)chunk*chunk_size);

		starpu_data_handle chunk_handle =
			starpu_data_get_child(root_handle, chunk);

		starpu_memory_node node;
		for (node = 0; node < starpu_memory_nodes_count(); node++)
		{
			starpu_matrix_interface_t *local =
				starpu_data_get_interface_on_node(chunk_handle, node);

			local->nx = nx;
			local->ny = child_ny;
			local->elemsize = elemsize;

			if (starpu_data_test_if_allocated_on_node(root_handle, node)) {
				starpu_matrix_interface_t *local_root =
					starpu_data_get_interface_on_node(root_handle, node);

				size_t offset = 
					(size_t)chunk*chunk_size*local_root->ld*elemsize;
				local->ptr = local_root->ptr + offset;
				local->ld = local_root->ld;
                                local->dev_handle = local_root->dev_handle;
                                local->offset = local_root->offset + offset;
			}
		}
	}
}
