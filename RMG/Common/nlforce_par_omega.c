/************************** SVN Revision Information **************************
 **    $Id$    **
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include "main.h"

void nlforce_par_omega (rmg_double_t * par_omega, int ion, int nh, rmg_double_t *force)
{
    int idx, idx1, size, n, m;
    rmg_double_t forces[3];
    rmg_double_t *omega_x, *omega_y, *omega_z, *qqq;

    size = nh * (nh + 1) / 2;

    omega_x = par_omega;
    omega_y = omega_x + size;
    omega_z = omega_y + size;

    qqq = pct.qqq[ion];

    for (idx = 0; idx < 3; idx++)
        forces[idx] = 0.0;

    idx = 0;
    for (n = 0; n < nh; n++)
    {
        for (m = n; m < nh; m++)
        {
            idx1 = n * nh + m;
            if (n == m)
            {
                forces[0] += qqq[idx1] * omega_x[idx];
                forces[1] += qqq[idx1] * omega_y[idx];
                forces[2] += qqq[idx1] * omega_z[idx];
            }
            else
            {
                forces[0] += 2.0 * qqq[idx1] * omega_x[idx];
                forces[1] += 2.0 * qqq[idx1] * omega_y[idx];
                forces[2] += 2.0 * qqq[idx1] * omega_z[idx];
            }

            ++idx;
        }
    }


    force[0] -= forces[0];
    force[1] -= forces[1];
    force[2] -= forces[2];

}
