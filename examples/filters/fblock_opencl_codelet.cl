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

__kernel void fblock_opencl(__global int* block, int nx, int ny, int nz, unsigned ldy, unsigned ldz, int factor)
{
        int i, j, k;
        for(k=0; k<nz ; k++) {
                for(j=0; j<ny ; j++) {
                        for(i=0; i<nx ; i++)
                                block[(k*ldz)+(j*ldy)+i] = factor;
                }
        }
}
