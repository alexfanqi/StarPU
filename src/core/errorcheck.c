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

#include <core/errorcheck.h>
#include <core/workers.h>

void set_local_worker_status(worker_status st)
{
	struct worker_s *worker = get_local_worker_key();
	STARPU_ASSERT(worker);

	worker->status = st;
}

worker_status get_local_worker_status(void)
{
	struct worker_s *worker = get_local_worker_key();
	if (STARPU_UNLIKELY(!worker))
		return STATUS_INVALID;

	return worker->status;
}

/* It is forbidden to call blocking operations with Callback and during the
 * execution of a task. */
unsigned worker_may_perform_blocking_calls(void)
{
	worker_status st = get_local_worker_status();

	return ( !(st == STATUS_CALLBACK) && !(st == STATUS_EXECUTING));
}