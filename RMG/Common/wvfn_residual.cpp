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


#include <stdio.h>
#include <time.h>
#include <math.h>
#include "main.h"




void wvfn_residual (STATE * states)
{
    int is, nspin = (ct.spin_flag + 1);
    double eigmean = 0.0;

    for (is = 0; is < ct.num_states; is++)
        if (states[is].occupation[0] > 0.1)
            eigmean += states[is].res;

    if (ct.spin_flag)
	    eigmean = real_sum_all (eigmean, pct.spin_comm);

    eigmean = eigmean / ((double) (ct.num_states * nspin));
    printf ("Mean occupied wavefunction residual = %14.6e\n", eigmean);

}