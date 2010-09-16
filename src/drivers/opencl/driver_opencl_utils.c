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

#include <starpu.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <starpu_opencl.h>
#include <core/workers.h>
#include "driver_opencl_utils.h"
#include "driver_opencl.h"

char *_starpu_opencl_program_dir;

#define _STARPU_STRINGIFY_(x) #x
#define _STARPU_STRINGIFY(x) _STARPU_STRINGIFY_(x)

static
int _starpu_opencl_locate_file(char *source_file_name, char *located_file_name) {
        _STARPU_DEBUG("Trying to locate <%s>\n", source_file_name);
        if (access(source_file_name, R_OK) == 0) {
                strcpy(located_file_name, source_file_name);
                return EXIT_SUCCESS;
        }
        if (_starpu_opencl_program_dir) {
                sprintf(located_file_name, "%s/%s", _starpu_opencl_program_dir, source_file_name);
                _STARPU_DEBUG("Trying to locate <%s>\n", located_file_name);
                if (access(located_file_name, R_OK) == 0) return EXIT_SUCCESS;
        }
        sprintf(located_file_name, "%s/%s", _STARPU_STRINGIFY(STARPU_OPENCL_DATADIR), source_file_name);
        _STARPU_DEBUG("Trying to locate <%s>\n", located_file_name);
        if (access(located_file_name, R_OK) == 0) return EXIT_SUCCESS;
        sprintf(located_file_name, "%s/%s", STARPU_SRC_DIR, source_file_name);
        _STARPU_DEBUG("Trying to locate <%s>\n", located_file_name);
        if (access(located_file_name, R_OK) == 0) return EXIT_SUCCESS;

        strcpy(located_file_name, "");
        _STARPU_ERROR("Cannot locate file <%s>\n", source_file_name);
        return EXIT_FAILURE;
}

int starpu_opencl_load_kernel(cl_kernel *kernel, cl_command_queue *queue, struct starpu_opencl_program *opencl_programs,
                              char *kernel_name, int devid)
{
        int err;
	cl_device_id device;
        cl_context context;
        cl_program program;

        starpu_opencl_get_device(devid, &device);
        starpu_opencl_get_context(devid, &context);
        starpu_opencl_get_queue(devid, queue);

        program = opencl_programs->programs[devid];
        if (!program) {
                _STARPU_DISP("Program not available\n");
                return CL_INVALID_PROGRAM;
        }

        // Create the compute kernel in the program we wish to run
        *kernel = clCreateKernel(program, kernel_name, &err);
	if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

	return CL_SUCCESS;
}

int starpu_opencl_release_kernel(cl_kernel kernel) {
	cl_int err;

	err = clReleaseKernel(kernel);
	if (err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

        return CL_SUCCESS;
}

static
char *_starpu_opencl_load_program_source(const char *filename)
{
        struct stat statbuf;
        FILE        *fh;
        char        *source;

        fh = fopen(filename, "r");
        if (fh == 0)
                return NULL;

        stat(filename, &statbuf);
        source = (char *) malloc(statbuf.st_size + 1);
        fread(source, statbuf.st_size, 1, fh);
        source[statbuf.st_size] = '\0';

        fclose(fh);

        return source;
}

int starpu_opencl_load_opencl_from_string(char *opencl_program_source, struct starpu_opencl_program *opencl_programs)
{
        unsigned int dev;
        unsigned int nb_devices;

        nb_devices = _starpu_opencl_get_device_count();
        // Iterate over each device
        for(dev = 0; dev < nb_devices; dev ++) {
                cl_device_id device;
                cl_context   context;
                cl_program   program;
                cl_int       err;

                starpu_opencl_get_device(dev, &device);
                starpu_opencl_get_context(dev, &context);
                opencl_programs->programs[dev] = NULL;

                if (context == NULL) continue;

                // Create the compute program from the source buffer
                program = clCreateProgramWithSource(context, 1, (const char **) &opencl_program_source, NULL, &err);
                if (!program || err != CL_SUCCESS) STARPU_OPENCL_REPORT_ERROR(err);

                // Build the program executable
                err = clBuildProgram(program, 1, &device, "-Werror -cl-mad-enable", NULL, NULL);
                if (err != CL_SUCCESS) {
                        size_t len;
                        static char buffer[4096];

                        _STARPU_DISP("Error: Failed to build program executable!\n");
                        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);

                        _STARPU_DISP("<%s>\n", buffer);
                        return EXIT_FAILURE;
                }

                // Store program
                opencl_programs->programs[dev] = program;
        }
        return EXIT_SUCCESS;
}

int starpu_opencl_load_opencl_from_file(char *source_file_name, struct starpu_opencl_program *opencl_programs)
{
        char located_file_name[1024];

        // Locate source file
        _starpu_opencl_locate_file(source_file_name, located_file_name);
        _STARPU_DEBUG("Source file name : <%s>\n", located_file_name);

        // Load the compute program from disk into a cstring buffer
        char *opencl_program_source = _starpu_opencl_load_program_source(located_file_name);
        if(!opencl_program_source)
                _STARPU_ERROR("Failed to load compute program from file <%s>!\n", located_file_name);

        return starpu_opencl_load_opencl_from_string(opencl_program_source, opencl_programs);
}

int starpu_opencl_unload_opencl(struct starpu_opencl_program *opencl_programs)
{
        unsigned int dev;
        unsigned int nb_devices;

        nb_devices = _starpu_opencl_get_device_count();
        // Iterate over each device
        for(dev = 0; dev < nb_devices; dev ++) {
                if (opencl_programs->programs[dev])
                        clReleaseProgram(opencl_programs->programs[dev]);
        }
        return EXIT_SUCCESS;
}
