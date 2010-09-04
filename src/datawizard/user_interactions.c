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

#include <common/config.h>
#include <common/utils.h>
#include <core/task.h>
#include <datawizard/coherency.h>
#include <datawizard/copy_driver.h>
#include <datawizard/write_back.h>
#include <core/dependencies/data_concurrency.h>

/* Explicitly ask StarPU to allocate room for a piece of data on the specified
 * memory node. */
int starpu_data_request_allocation(starpu_data_handle handle, uint32_t node)
{
	starpu_data_request_t r;

	STARPU_ASSERT(handle);

	r = _starpu_create_data_request(handle, 0, node, node, 0, 1);

	/* we do not increase the refcnt associated to the request since we are
	 * not waiting for its termination */

	_starpu_post_data_request(r, node);

	return 0;
}

struct user_interaction_wrapper {
	starpu_data_handle handle;
	starpu_access_mode mode;
	unsigned node;
	pthread_cond_t cond;
	pthread_mutex_t lock;
	unsigned finished;
	unsigned async;
	void (*callback)(void *);
	void (*callback_fetch_data)(void *); // called after fetch_data
	void *callback_arg;
	struct starpu_task *pre_sync_task;
	struct starpu_task *post_sync_task;
};

/*
 *	Non Blocking data request from application
 */
/* put the current value of the data into RAM */
static void _starpu_data_acquire_fetch_data_callback(void *arg)
{
	struct user_interaction_wrapper *wrapper = arg;
	starpu_data_handle handle = wrapper->handle;

	/* At that moment, the caller holds a reference to the piece of data.
	 * We enqueue the "post" sync task in the list associated to the handle
	 * so that it is submitted by the starpu_data_release
	 * function. */
	_starpu_add_post_sync_tasks(wrapper->post_sync_task, handle);

	wrapper->callback(wrapper->callback_arg);
}

static void _starpu_data_acquire_continuation_non_blocking(void *arg)
{
	int ret;
	struct user_interaction_wrapper *wrapper = arg;

	starpu_data_handle handle = wrapper->handle;

	STARPU_ASSERT(handle);

	ret = _starpu_fetch_data_on_node(handle, 0, wrapper->mode, 1,
			_starpu_data_acquire_fetch_data_callback, wrapper);
	STARPU_ASSERT(!ret);
}

static void starpu_data_acquire_cb_pre_sync_callback(void *arg)
{
	struct user_interaction_wrapper *wrapper = arg;

	/* we try to get the data, if we do not succeed immediately, we set a
 	* callback function that will be executed automatically when the data is
 	* available again, otherwise we fetch the data directly */
	if (!_starpu_attempt_to_submit_data_request_from_apps(wrapper->handle, wrapper->mode,
			_starpu_data_acquire_continuation_non_blocking, wrapper))
	{
		/* no one has locked this data yet, so we proceed immediately */
		_starpu_data_acquire_continuation_non_blocking(wrapper);
	}
}

/* The data must be released by calling starpu_data_release later on */
int starpu_data_acquire_cb(starpu_data_handle handle,
		starpu_access_mode mode, void (*callback)(void *), void *arg)
{
	STARPU_ASSERT(handle);

	struct user_interaction_wrapper *wrapper = malloc(sizeof(struct user_interaction_wrapper));
	STARPU_ASSERT(wrapper);

	wrapper->handle = handle;
	wrapper->mode = mode;
	wrapper->callback = callback;
	wrapper->callback_arg = arg;
	PTHREAD_COND_INIT(&wrapper->cond, NULL);
	PTHREAD_MUTEX_INIT(&wrapper->lock, NULL);
	wrapper->finished = 0;

#warning TODO instead of having the is_prefetch argument, _starpu_fetch_data shoud consider two flags: async and detached
	_starpu_spin_lock(&handle->header_lock);
	handle->per_node[0].refcnt++;
	_starpu_spin_unlock(&handle->header_lock);

	PTHREAD_MUTEX_LOCK(&handle->sequential_consistency_mutex);
	int sequential_consistency = handle->sequential_consistency;
	if (sequential_consistency)
	{
		wrapper->pre_sync_task = starpu_task_create();
		wrapper->pre_sync_task->detach = 1;
		wrapper->pre_sync_task->callback_func = starpu_data_acquire_cb_pre_sync_callback;
		wrapper->pre_sync_task->callback_arg = wrapper;

		wrapper->post_sync_task = starpu_task_create();
		wrapper->post_sync_task->detach = 1;

		_starpu_detect_implicit_data_deps_with_handle(wrapper->pre_sync_task, wrapper->post_sync_task, handle, mode);
		PTHREAD_MUTEX_UNLOCK(&handle->sequential_consistency_mutex);

		/* TODO detect if this is superflous */
		int ret = starpu_task_submit(wrapper->pre_sync_task);
		STARPU_ASSERT(!ret);
	}
	else {
		PTHREAD_MUTEX_UNLOCK(&handle->sequential_consistency_mutex);

		starpu_data_acquire_cb_pre_sync_callback(wrapper);
	}

	return 0;
}

/*
 *	Block data request from application
 */
static inline void _starpu_data_acquire_continuation(void *arg)
{
	struct user_interaction_wrapper *wrapper = arg;

	starpu_data_handle handle = wrapper->handle;

	STARPU_ASSERT(handle);

	_starpu_fetch_data_on_node(handle, 0, wrapper->mode, 0, NULL, NULL);
	
	/* continuation of starpu_data_acquire */
	PTHREAD_MUTEX_LOCK(&wrapper->lock);
	wrapper->finished = 1;
	PTHREAD_COND_SIGNAL(&wrapper->cond);
	PTHREAD_MUTEX_UNLOCK(&wrapper->lock);
}

/* The data must be released by calling starpu_data_release later on */
int starpu_data_acquire(starpu_data_handle handle, starpu_access_mode mode)
{
	STARPU_ASSERT(handle);

	/* it is forbidden to call this function from a callback or a codelet */
	if (STARPU_UNLIKELY(!_starpu_worker_may_perform_blocking_calls()))
		return -EDEADLK;

	struct user_interaction_wrapper wrapper =
	{
		.handle = handle,
		.mode = mode,
		.node = 0, // unused
		.cond = PTHREAD_COND_INITIALIZER,
		.lock = PTHREAD_MUTEX_INITIALIZER,
		.finished = 0
	};

//	fprintf(stderr, "TAKE sequential_consistency_mutex starpu_data_acquire\n");
	PTHREAD_MUTEX_LOCK(&handle->sequential_consistency_mutex);
	int sequential_consistency = handle->sequential_consistency;
	if (sequential_consistency)
	{
		wrapper.pre_sync_task = starpu_task_create();
		wrapper.pre_sync_task->detach = 0;

		wrapper.post_sync_task = starpu_task_create();
		wrapper.post_sync_task->detach = 1;

		_starpu_detect_implicit_data_deps_with_handle(wrapper.pre_sync_task, wrapper.post_sync_task, handle, mode);
		PTHREAD_MUTEX_UNLOCK(&handle->sequential_consistency_mutex);

		/* TODO detect if this is superflous */
		wrapper.pre_sync_task->synchronous = 1;
		int ret = starpu_task_submit(wrapper.pre_sync_task);
		STARPU_ASSERT(!ret);
		//starpu_task_wait(wrapper.pre_sync_task);
	}
	else {
		PTHREAD_MUTEX_UNLOCK(&handle->sequential_consistency_mutex);
	}

	/* we try to get the data, if we do not succeed immediately, we set a
 	* callback function that will be executed automatically when the data is
 	* available again, otherwise we fetch the data directly */
	if (!_starpu_attempt_to_submit_data_request_from_apps(handle, mode,
			_starpu_data_acquire_continuation, &wrapper))
	{
		/* no one has locked this data yet, so we proceed immediately */
		int ret = _starpu_fetch_data_on_node(handle, 0, mode, 0, NULL, NULL);
		STARPU_ASSERT(!ret);
	}
	else {
		PTHREAD_MUTEX_LOCK(&wrapper.lock);
		while (!wrapper.finished)
			PTHREAD_COND_WAIT(&wrapper.cond, &wrapper.lock);
		PTHREAD_MUTEX_UNLOCK(&wrapper.lock);
	}

	/* At that moment, the caller holds a reference to the piece of data.
	 * We enqueue the "post" sync task in the list associated to the handle
	 * so that it is submitted by the starpu_data_release
	 * function. */
	_starpu_add_post_sync_tasks(wrapper.post_sync_task, handle);

	return 0;
}

/* This function must be called after starpu_data_acquire so that the
 * application release the data */
void starpu_data_release(starpu_data_handle handle)
{
	STARPU_ASSERT(handle);

	/* The application can now release the rw-lock */
	_starpu_release_data_on_node(handle, 0, 0);

	/* In case there are some implicit dependencies, unlock the "post sync" tasks */
	_starpu_unlock_post_sync_tasks(handle);
}

static void _prefetch_data_on_node(void *arg)
{
	struct user_interaction_wrapper *wrapper = arg;
        int ret;

	ret = _starpu_fetch_data_on_node(wrapper->handle, wrapper->node, STARPU_R, wrapper->async, NULL, NULL);
        STARPU_ASSERT(!ret);

        PTHREAD_MUTEX_LOCK(&wrapper->lock);
	wrapper->finished = 1;
	PTHREAD_COND_SIGNAL(&wrapper->cond);
	PTHREAD_MUTEX_UNLOCK(&wrapper->lock);

	if (!wrapper->async)
	{
		_starpu_spin_lock(&wrapper->handle->header_lock);
		_starpu_notify_data_dependencies(wrapper->handle);
		_starpu_spin_unlock(&wrapper->handle->header_lock);
	}

}

int _starpu_prefetch_data_on_node_with_mode(starpu_data_handle handle, unsigned node, unsigned async, starpu_access_mode mode)
{
	STARPU_ASSERT(handle);

	/* it is forbidden to call this function from a callback or a codelet */
	if (STARPU_UNLIKELY(!_starpu_worker_may_perform_blocking_calls()))
		return -EDEADLK;

	struct user_interaction_wrapper wrapper =
	{
		.handle = handle,
		.node = node,
		.async = async,
		.cond = PTHREAD_COND_INITIALIZER,
		.lock = PTHREAD_MUTEX_INITIALIZER,
		.finished = 0
	};

	if (!_starpu_attempt_to_submit_data_request_from_apps(handle, mode, _prefetch_data_on_node, &wrapper))
	{
		/* we can immediately proceed */
		_starpu_fetch_data_on_node(handle, node, mode, async, NULL, NULL);

		/* remove the "lock"/reference */
		if (!async)
		{
			_starpu_spin_lock(&handle->header_lock);
			_starpu_notify_data_dependencies(handle);
			_starpu_spin_unlock(&handle->header_lock);
		}
	}
	else {
		PTHREAD_MUTEX_LOCK(&wrapper.lock);
		while (!wrapper.finished)
			PTHREAD_COND_WAIT(&wrapper.cond, &wrapper.lock);
		PTHREAD_MUTEX_UNLOCK(&wrapper.lock);
	}

	return 0;
}

int starpu_data_prefetch_on_node(starpu_data_handle handle, unsigned node, unsigned async)
{
	return _starpu_prefetch_data_on_node_with_mode(handle, node, async, STARPU_R);
}

/*
 *	It is possible to specify that a piece of data can be discarded without
 *	impacting the application.
 */
void starpu_data_advise_as_important(starpu_data_handle handle, unsigned is_important)
{
	_starpu_spin_lock(&handle->header_lock);

	/* first take all the children lock (in order !) */
	unsigned child;
	for (child = 0; child < handle->nchildren; child++)
	{
		/* make sure the intermediate children is advised as well */
		struct starpu_data_state_t *child_handle = &handle->children[child];
		if (child_handle->nchildren > 0)
			starpu_data_advise_as_important(child_handle, is_important);
	}

	handle->is_not_important = !is_important;

	/* now the parent may be used again so we release the lock */
	_starpu_spin_unlock(&handle->header_lock);

}

void starpu_data_set_sequential_consistency_flag(starpu_data_handle handle, unsigned flag)
{
	_starpu_spin_lock(&handle->header_lock);

	unsigned child;
	for (child = 0; child < handle->nchildren; child++)
	{
		/* make sure that the flags are applied to the children as well */
		struct starpu_data_state_t *child_handle = &handle->children[child];
		if (child_handle->nchildren > 0)
			starpu_data_set_sequential_consistency_flag(child_handle, flag);
	}

	PTHREAD_MUTEX_LOCK(&handle->sequential_consistency_mutex);
	handle->sequential_consistency = flag;
	PTHREAD_MUTEX_UNLOCK(&handle->sequential_consistency_mutex);

	_starpu_spin_unlock(&handle->header_lock);
}

/* By default, sequential consistency is enabled */
static unsigned default_sequential_consistency_flag = 1;

unsigned starpu_data_get_default_sequential_consistency_flag(void)
{
	return default_sequential_consistency_flag;
}

void starpu_data_set_default_sequential_consistency_flag(unsigned flag)
{
	default_sequential_consistency_flag = flag;
}

/* Query the status of the handle on the specified memory node. */
void starpu_data_query_status(starpu_data_handle handle, int memory_node, int *is_allocated, int *is_valid, int *is_requested)
{
	_starpu_spin_lock(&handle->header_lock);

	if (is_allocated)
		*is_allocated = handle->per_node[memory_node].allocated;

	if (is_valid)
		*is_valid = (handle->per_node[memory_node].state != STARPU_INVALID);

	if (is_requested)
		*is_requested = handle->per_node[memory_node].requested;

	_starpu_spin_unlock(&handle->header_lock);
}
