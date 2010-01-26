/*
 * StarPU
 * Copyright (C) INRIA 2008-2010 (see AUTHORS file)
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

/* This creates (and submits) an empty task that unlocks a tag once all its
 * dependencies are fulfilled. */
void starpu_create_sync_task(starpu_tag_t sync_tag, unsigned ndeps, starpu_tag_t *deps)
{
	starpu_tag_declare_deps_array(sync_tag, ndeps, deps);

	/* We create an empty task */
	struct starpu_task *sync_task = starpu_task_create();

	sync_task->use_tag = 1;
	sync_task->tag_id = sync_tag;

	/* This task does nothing */
	sync_task->cl = NULL;

	int sync_ret = starpu_submit_task(sync_task);
	STARPU_ASSERT(!sync_ret);
}
