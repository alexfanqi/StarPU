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

#include <common/config.h>
#include <core/debug.h>
#include <common/utils.h>

#ifdef STARPU_VERBOSE
/* we want a single writer at the same time to have a log that is readable */
static pthread_mutex_t logfile_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE *logfile;
#endif

void _starpu_open_debug_logfile(void)
{
#ifdef STARPU_VERBOSE
	/* what is  the name of the file ? default = "starpu.log" */
	char *logfile_name;
	
	logfile_name = getenv("STARPU_LOGFILENAME");
	if (!logfile_name)
	{
		logfile_name = "starpu.log";
	}

	logfile = fopen(logfile_name, "w+");
	STARPU_ASSERT(logfile);
#endif
}

void _starpu_close_debug_logfile(void)
{
#ifdef STARPU_VERBOSE
	fclose(logfile);
#endif
}

void _starpu_print_to_logfile(const char *format __attribute__((unused)), ...)
{
#ifdef STARPU_VERBOSE
	va_list args;
	va_start(args, format);
	PTHREAD_MUTEX_LOCK(&logfile_mutex);
	vfprintf(logfile, format, args);
	PTHREAD_MUTEX_UNLOCK(&logfile_mutex);
	va_end( args );
#endif
}
