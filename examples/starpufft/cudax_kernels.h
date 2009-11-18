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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU Lesser General Public License in COPYING.LGPL for more details.
 */

#include <cuComplex.h>
_externC void STARPUFFT(cuda_twist1_1d_host)(const _cuComplex *in, _cuComplex *twisted1, unsigned i, unsigned n1, unsigned n2, cudaStream_t stream);
_externC void STARPUFFT(cuda_twiddle_1d_host)(_cuComplex *out, const _cuComplex *roots, unsigned n, unsigned i, cudaStream_t stream);
_externC void STARPUFFT(cuda_twist1_2d_host)(const _cuComplex *in, _cuComplex *twisted1, unsigned i, unsigned j, unsigned n1, unsigned n2, unsigned m1, unsigned m2, cudaStream_t stream);
_externC void STARPUFFT(cuda_twiddle_2d_host)(_cuComplex *out, const _cuComplex *roots0, const _cuComplex *roots1, unsigned n2, unsigned m2, unsigned i, unsigned j, cudaStream_t stream);