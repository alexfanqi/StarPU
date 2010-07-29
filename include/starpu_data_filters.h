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

#ifndef __STARPU_DATA_FILTERS_H__
#define __STARPU_DATA_FILTERS_H__

#include <starpu.h>
#include <starpu_config.h>

#ifdef __cplusplus
extern "C" {
#endif

struct starpu_data_interface_ops_t;

struct starpu_data_filter {
	void (*filter_func)(void *father_interface, void *child_interface, struct starpu_data_filter *, unsigned id, unsigned nparts);
        unsigned (*get_nchildren)(struct starpu_data_filter *, starpu_data_handle initial_handle);
        struct starpu_data_interface_ops_t *(*get_child_ops)(struct starpu_data_filter *, unsigned id);
        unsigned filter_arg;
        unsigned nchildren;
        void *filter_arg_ptr;
};

void starpu_data_partition(starpu_data_handle initial_handle, struct starpu_data_filter *f);
void starpu_data_unpartition(starpu_data_handle root_data, uint32_t gathering_node);

int starpu_data_get_nb_children(starpu_data_handle handle);
starpu_data_handle starpu_data_get_child(starpu_data_handle handle, unsigned i);

/* unsigned list */
starpu_data_handle starpu_data_get_sub_data(starpu_data_handle root_data, unsigned depth, ... );

/* struct starpu_data_filter * list */
void starpu_data_map_filters(starpu_data_handle root_data, unsigned nfilters, ...);

/* a few examples of filters */

/* for BCSR */
void starpu_canonical_block_filter_bcsr(void *father_interface, void *child_interface, struct starpu_data_filter *f, unsigned id, unsigned nparts);
void starpu_vertical_block_filter_func_csr(void *father_interface, void *child_interface, struct starpu_data_filter *f, unsigned id, unsigned nparts);
/* (filters for BLAS interface) */
void starpu_block_filter_func(void *father_interface, void *child_interface, struct starpu_data_filter *f, unsigned id, unsigned nparts);
void starpu_vertical_block_filter_func(void *father_interface, void *child_interface, struct starpu_data_filter *f, unsigned id, unsigned nparts);

/* for vector */
void starpu_block_filter_func_vector(void *father_interface, void *child_interface, struct starpu_data_filter *f, unsigned id, unsigned nparts);
void starpu_vector_list_filter_func(void *father_interface, void *child_interface, struct starpu_data_filter *f, unsigned id, unsigned nparts);
void starpu_vector_divide_in_2_filter_func(void *father_interface, void *child_interface, struct starpu_data_filter *f, unsigned id, unsigned nparts);

/* for block */
void starpu_block_filter_func_block(void *father_interface, void *child_interface, struct starpu_data_filter *f, unsigned id, unsigned nparts);

#ifdef __cplusplus
}
#endif

#endif
