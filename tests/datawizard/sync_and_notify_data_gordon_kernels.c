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

void incA (__attribute__ ((unused)) void **alloc,
                __attribute__ ((unused)) void **in,
                __attribute__ ((unused)) void **inout,
                __attribute__ ((unused)) void **out)
{
	unsigned *v = inout[0];
	v[0]++;
}

void incC (__attribute__ ((unused)) void **alloc,
                __attribute__ ((unused)) void **in,
                __attribute__ ((unused)) void **inout,
                __attribute__ ((unused)) void **out)
{
	unsigned *v = inout[0];
	v[2]++;
}
