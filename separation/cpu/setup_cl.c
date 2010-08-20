/* Copyright 2010 Matthew Arsenault, Travis Desell, Boleslaw
Szymanski, Heidi Newberg, Carlos Varela, Malik Magdon-Ismail and
Rensselaer Polytechnic Institute.

This file is part of Milkway@Home.

Milkyway@Home is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Milkyway@Home is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Milkyway@Home.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <OpenCL/cl.h>
#include <OpenCL/cl_ext.h>

#include "milkyway_util.h"
#include "show_cl_types.h"
#include "setup_cl.h"
#include "separation_cl_buffers.h"
#include "separation_cl_defs.h"

#define BUFSIZE 4096

inline static cl_int separationSetKernelArgs(const ASTRONOMY_PARAMETERS* ap,
                                             const INTEGRAL_AREA* ia,
                                             const CLInfo* ci,
                                             SeparationCLMem* cm)
{
    cl_int err = CL_SUCCESS;

    /* Output buffer */
    err |= clSetKernelArg(ci->kern, 0, sizeof(cl_mem), &cm->outNu);

    /* The constant arguments */
    err |= clSetKernelArg(ci->kern, 1, sizeof(ASTRONOMY_PARAMETERS), ap);
    err |= clSetKernelArg(ci->kern, 2, sizeof(INTEGRAL_AREA), ia);
    err |= clSetKernelArg(ci->kern, 3, sizeof(cl_mem), &cm->sc);
    err |= clSetKernelArg(ci->kern, 4, sizeof(cl_mem), &cm->sg);
    err |= clSetKernelArg(ci->kern, 5, sizeof(cl_mem), &cm->nuConsts);

    /* Local workspaces */
    err |= clSetKernelArg(ci->kern, 6, sizeof(ST_PROBS) * ap->number_streams, NULL); /* st_probs */
    err |= clSetKernelArg(ci->kern, 7, sizeof(vector) * ap->convolve, NULL);         /* xyz */
    err |= clSetKernelArg(ci->kern, 8, sizeof(R_POINTS) * ap->convolve, NULL);       /* r_pts */

    if (err != CL_SUCCESS)
    {
        warn("Error setting kernel arguments: %s\n", showCLInt(err));
        return err;
    }

    return CL_SUCCESS;
}

cl_int setupSeparationCL(const ASTRONOMY_PARAMETERS* ap,
                         const INTEGRAL_AREA* ia,
                         const STREAM_CONSTANTS* sc,
                         const STREAM_GAUSS* sg,
                         const NU_CONSTANTS* nu_consts,
                         CLInfo* ci,
                         SeparationCLMem* cm)
{
    cl_int err;
    char* compileDefs;
    char* kernelSrc;

    kernelSrc = mwReadFile("/Users/matt/src/milkywayathome_client/separation/kernels/integrals.cl");


    compileDefs = separationCLDefs(ap,
                                   "-DDOUBLEPREC=1 "
                                   "-DHAVE_SINCOS=1 "
                                   "-I/Users/matt/src/milkywayathome_client/separation/cpu "
                                   "-I/Users/matt/src/milkywayathome_client/separation/include "
                                   "-I/Users/matt/src/milkywayathome_client/milkyway/include "
                                   );

    err = getCLInfo(ci, CL_DEVICE_TYPE_CPU, "r_sum_kernel", &kernelSrc, compileDefs);

    free(kernelSrc);
    free(compileDefs);

    if (err != CL_SUCCESS)
    {
        fail("Failed to setup OpenCL device: %s\n", showCLInt(err));
        return err;
    }

    err = createSeparationBuffers(ap, ia, sc, sg, nu_consts, ci, cm);
    if (err != CL_SUCCESS)
    {
        fail("Failed to create CL buffers: %s\n", showCLInt(err));
        return err;
    }

    err = separationSetKernelArgs(ap, ia, ci, cm);
    if (err != CL_SUCCESS)
    {
        fail("Failed to set integral kernel arguments: %s\n", showCLInt(err));
        return err;
    }

    return CL_SUCCESS;
}

