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

#ifndef __DRIVER_OPENCL_H__
#define __DRIVER_OPENCL_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef STARPU_USE_OPENCL

#include <CL/cl.h>

extern
int _starpu_opencl_init_context(int devid);

extern
int _starpu_opencl_deinit_context(int devid);

extern
unsigned _starpu_opencl_get_device_count(void);

extern
int _starpu_opencl_allocate_memory(void **addr, size_t size, cl_mem_flags flags);

extern
int _starpu_opencl_copy_ram_to_opencl(void *ptr, cl_mem buffer, size_t size, size_t offset, cl_event *event);

extern
int _starpu_opencl_copy_opencl_to_ram(cl_mem buffer, void *ptr, size_t size, size_t offset, cl_event *event);

extern
int _starpu_opencl_copy_ram_to_opencl_async_sync(void *ptr, cl_mem buffer, size_t size, size_t offset, cl_event *event, int *ret);

extern
int _starpu_opencl_copy_opencl_to_ram_async_sync(cl_mem buffer, void *ptr, size_t size, size_t offset, cl_event *event, int *ret);

#if 0
extern
int _starpu_opencl_copy_rect_opencl_to_ram(cl_mem buffer, void *ptr, const size_t buffer_origin[3], const size_t host_origin[3],
                                           const size_t region[3], size_t buffer_row_pitch, size_t buffer_slice_pitch,
                                           size_t host_row_pitch, size_t host_slice_pitch, cl_event *event);

extern
int _starpu_opencl_copy_rect_ram_to_opencl(void *ptr, cl_mem buffer, const size_t buffer_origin[3], const size_t host_origin[3],
                                           const size_t region[3], size_t buffer_row_pitch, size_t buffer_slice_pitch,
                                           size_t host_row_pitch, size_t host_slice_pitch, cl_event *event);
#endif

extern
void _starpu_opencl_init(void);

extern
void *_starpu_opencl_worker(void *);

#endif // STARPU_USE_OPENCL
#endif //  __DRIVER_OPENCL_H__
