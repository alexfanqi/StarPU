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

void starpu_vertical_block_filter_func_csr(starpu_filter *f, starpu_data_handle root_handle)
{
	unsigned nchunks;
	uint32_t arg = f->filter_arg;

	starpu_csr_interface_t *root_interface =
		starpu_data_get_interface_on_node(root_handle, 0);

	uint32_t nrow = root_interface->nrow;
	size_t elemsize = root_interface->elemsize;
	uint32_t firstentry = root_interface->firstentry;

	/* we will have arg chunks */
	nchunks = STARPU_MIN(nrow, arg);
	
	/* first allocate the children (they also use the csr interface) */
	starpu_data_create_children(root_handle, nchunks, root_handle->ops);

	/* actually create all the chunks */
	uint32_t chunk_size = (nrow + nchunks - 1)/nchunks;

	STARPU_ASSERT(starpu_data_test_if_allocated_on_node(root_handle, 0));
	uint32_t *rowptr = root_interface->rowptr;

	unsigned chunk;
	for (chunk = 0; chunk < nchunks; chunk++)
	{
		uint32_t first_index = chunk*chunk_size - firstentry;
		uint32_t local_firstentry = rowptr[first_index];

		uint32_t child_nrow = 
			STARPU_MIN(chunk_size, nrow - chunk*chunk_size);

		uint32_t local_nnz = rowptr[first_index + child_nrow] - rowptr[first_index]; 

		starpu_data_handle chunk_handle =
			starpu_data_get_child(root_handle, chunk);

		starpu_memory_node node;
		for (node = 0; node < starpu_memory_nodes_count(); node++)
		{
			starpu_csr_interface_t *local = 
				starpu_data_get_interface_on_node(chunk_handle, node);

			starpu_csr_interface_t *root_local = 
				starpu_data_get_interface_on_node(root_handle, node);

			local->nnz = local_nnz;
			local->nrow = child_nrow;
			local->firstentry = local_firstentry;
			local->elemsize = elemsize;

			if (starpu_data_test_if_allocated_on_node(root_handle, node)) {
				local->rowptr = &root_local->rowptr[first_index];
				local->colind = &root_local->colind[local_firstentry];
				local->nzval = root_local->nzval + local_firstentry * elemsize;
			}
		}
	}
}
