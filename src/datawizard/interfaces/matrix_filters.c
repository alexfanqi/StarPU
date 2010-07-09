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
void starpu_block_filter_func(void *father_interface, void *child_interface, __attribute__((unused)) starpu_filter *f, unsigned id, unsigned nchunks)
{
       starpu_matrix_interface_t *matrix_father = father_interface;
       starpu_matrix_interface_t *matrix_child = child_interface;
  
	uint32_t nx = matrix_father->nx;
	uint32_t ny = matrix_father->ny;
	size_t elemsize = matrix_father->elemsize;

	size_t chunk_size = ((size_t)nx + nchunks - 1)/nchunks;
	size_t offset = (size_t)id*chunk_size*elemsize;
	
	uint32_t child_nx = 
	  STARPU_MIN(chunk_size, (size_t)nx - (size_t)id*chunk_size);
	
	/* update the child's interface */
	matrix_child->nx = child_nx;
	matrix_child->ny = ny;
	matrix_child->elemsize = elemsize;
	
	/* is the information on this node valid ? */
	if (matrix_father->ptr) {
	  matrix_child->ptr = matrix_father->ptr + offset;
	  matrix_child->ld = matrix_father->ld;
	  matrix_child->dev_handle = matrix_father->dev_handle;
	  matrix_child->offset = matrix_father->offset + offset;
	}
}

void starpu_vertical_block_filter_func(void *father_interface, void *child_interface, __attribute__((unused)) starpu_filter *f, unsigned id, unsigned nchunks)
{
        starpu_matrix_interface_t *matrix_father = father_interface;
        starpu_matrix_interface_t *matrix_child = child_interface;

	uint32_t nx = matrix_father->nx;
	uint32_t ny = matrix_father->ny;
	size_t elemsize = matrix_father->elemsize;

	size_t chunk_size = ((size_t)ny + nchunks - 1)/nchunks;
	size_t child_ny = 
	  STARPU_MIN(chunk_size, (size_t)ny - (size_t)id*chunk_size);

	matrix_child->nx = nx;
	matrix_child->ny = child_ny;
	matrix_child->elemsize = elemsize;

	/* is the information on this node valid ? */
	if (matrix_father->ptr) {
	  size_t offset = 
	    (size_t)id*chunk_size*matrix_father->ld*elemsize;
	  matrix_child->ptr = matrix_father->ptr + offset;
	  matrix_child->ld = matrix_father->ld;
	  matrix_child->dev_handle = matrix_father->dev_handle;
	  matrix_child->offset = matrix_father->offset + offset;
	}
}
