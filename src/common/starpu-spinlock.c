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

#include <common/starpu-spinlock.h>
#include <common/config.h>

int starpu_spin_init(starpu_spinlock_t *lock)
{
#ifdef HAVE_PTHREAD_SPIN_LOCK
	return pthread_spin_init(&lock->lock, 0);
#else
	lock->taken = 0;
	return 0;
#endif
}

int starpu_spin_destroy(starpu_spinlock_t *lock)
{
#ifdef HAVE_PTHREAD_SPIN_LOCK
	return pthread_spin_destroy(&lock->lock);
#else
	/* we don't do anything */
	return 0;
#endif
}

int starpu_spin_lock(starpu_spinlock_t *lock)
{
#ifdef HAVE_PTHREAD_SPIN_LOCK
	return pthread_spin_lock(&lock->lock);
#else
	uint32_t prev;
	do {
		prev = __sync_lock_test_and_set(&lock->taken, 1);
	} while (prev);
	return 0;
#endif
}

int starpu_spin_trylock(starpu_spinlock_t *lock)
{
#ifdef HAVE_PTHREAD_SPIN_LOCK
	return pthread_spin_trylock(&lock->lock);
#else
	uint32_t prev;
	prev = __sync_lock_test_and_set(&lock->taken, 1);
	return (prev == 0)?0:EBUSY;
#endif
}

int starpu_spin_unlock(starpu_spinlock_t *lock)
{
#ifdef HAVE_PTHREAD_SPIN_LOCK
	return pthread_spin_unlock(&lock->lock);
#else
	__sync_lock_release(&lock->taken);
	return 0;
#endif
}
