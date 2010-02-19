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

#ifndef __DRIVER_GORDON_H__
#define __DRIVER_GORDON_H__

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>

#include <core/jobs.h>
#include <common/config.h>

#include <starpu.h>

#include <stdint.h>
#include <stdlib.h>

#include <libspe2.h>

#define NMAXGORDONSPUS	8

void *_starpu_gordon_worker(void *);

#endif // __DRIVER_GORDON_H__
