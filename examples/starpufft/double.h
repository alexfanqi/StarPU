/*
 * StarPU
 * Copyright (C) INRIA 2009 (see AUTHORS file)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR in PARTICULAR PURPOSE.
 *
 * See the GNU Lesser General Public License in COPYING.LGPL for more details.
 */

#include <complex.h>
#include <starpu_config.h>

#ifdef HAVE_FFTW
#include <fftw3.h>
#endif

#ifdef STARPU_USE_CUDA
#include <cufft.h>
#endif

typedef double real;
#ifdef HAVE_FFTW
typedef fftw_complex _fftw_complex;
typedef fftw_plan _fftw_plan;
#endif
#ifdef STARPU_USE_CUDA
typedef cuDoubleComplex _cuComplex;
typedef cufftDoubleComplex _cufftComplex;
#define _cufftExecC2C cufftExecZ2Z
#define _cufftExecR2C cufftExecD2Z
#define _cufftExecC2R cufftExecZ2D
#define _CUFFT_C2C CUFFT_Z2Z
#define _CUFFT_R2C CUFFT_D2Z
#define _CUFFT_C2R CUFFT_Z2D
#define _cuCmul(x,y) cuCmul(x,y)
#endif
#define STARPUFFT(name) starpufft_##name
#define _FFTW(name) fftw_##name

#define TYPE ""
