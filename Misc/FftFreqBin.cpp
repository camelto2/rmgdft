/*
 *
 * Copyright 2014 The RMG Project Developers. See the COPYRIGHT file 
 * at the top-level directory of this distribution or in the current
 * directory.
 * 
 * This file is part of RMG. 
 * RMG is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * any later version.
 *
 * RMG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
*/

#include <math.h>
#include <float.h>
#include <complex>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>

#include "const.h"
#include "rmgtypedefs.h"
#include "typedefs.h"
#include "xc.h"
#include "RmgSumAll.h"
#include "transition.h"
#include "GlobalSums.h"

#if USE_PFFT

#include "RmgParallelFft.h"

// On input performs a dft of x which is an array distributed in real space across all nodes
// using the plane wave structure defined in pwaves. It then computes a frequency histogram
// of all of the coefficients of the transform.
void FftFreqBin(double *x,   // IN:OUT  Input array in real space. Distributed across all nodes.
               Pw &pwaves,  // IN:     Plane wave structure that corresponds to the reciprocal space grid for x
               double *bins)  // IN:OUT   Allocated by calling array. Stores frequency bins. Calling array is
                              // respsponsible for making sure the array is big enough ((int)rint(pwaves.gmax) + 1)
                              // normalized so that \sum bins = 1.0
{

  ptrdiff_t grid[3];
  pfft_plan plan_forward;
  grid[0] = pwaves.global_dimx;
  grid[1] = pwaves.global_dimy;
  grid[2] = pwaves.global_dimz;

  int pbasis = pwaves.pbasis;
  int nvecs = (int)rint(pwaves.gmax) + 1;

  for(int i=0;i < nvecs;i++)bins[i] = 0.0;

  std::complex<double> *cvec = new std::complex<double>[pbasis];
  plan_forward = pfft_plan_dft_3d(grid,
                                  (double (*)[2])cvec,
                                  (double (*)[2])cvec,
                                  pct.pfft_comm,
                                  PFFT_FORWARD,
                                  PFFT_TRANSPOSED_NONE|PFFT_ESTIMATE);

  for(int i = 0;i < pbasis;i++) cvec[i] = std::complex<double>(x[i], 0.0);
  pfft_execute_dft(plan_forward, (double (*)[2])cvec, (double (*)[2])cvec);

  double bnorm = 0.0;
  for(int ig=0;ig < pbasis;ig++) {
      double t1 = std::norm(cvec[ig]);
      int gidx = (int)rint(pwaves.gmags[ig]);
      if(gidx < nvecs) bins[gidx] += t1;
      bnorm += t1;
  }

  GlobalSums (bins, nvecs, pct.grid_comm); 
  bnorm = 1.0 / RmgSumAll(bnorm, pct.grid_comm);

  for(int i = 0;i < nvecs;i++)
      bins[i] *= bnorm;

  pfft_destroy_plan(plan_forward);
  delete [] cvec;
}

#endif
