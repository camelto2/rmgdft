/************************** SVN Revision Information **************************
 **    $Id$    **
******************************************************************************/

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"

/*This function calculates atomic wavefunctions using wavefunctions read from PP files
 * with angular part added. The result is in psi, which is assumed to be initialized to zero*/

void get_awave (REAL *psi, ION *iptr, int awave_idx, int l, int m)
{

    int ix, iy, iz;
    int ion, idx, yindex;
    int ilow, jlow, klow, ihi, jhi, khi, map;
    int Aix[NX_GRID], Aiy[NY_GRID], Aiz[NZ_GRID];
    int icount, n, incx;
    int *pvec;
    REAL r, xc, yc, zc, vector[3];
    REAL x[3], invdr, t1, t2, xstart, ystart, zstart;;
    SPECIES *sp;

    /* Grab some memory for temporary storage */
    my_malloc (pvec, P0_BASIS, int);

    /* Get species type */
    sp = &ct.sp[iptr->species];


    /* Determine mapping indices or even if a mapping exists */
    map = get_index (pct.gridpe, iptr, Aix, Aiy, Aiz, &ilow, &ihi, &jlow, &jhi, &klow, &khi,
	    sp->adim_wave, PX0_GRID, PY0_GRID, PZ0_GRID,
	    ct.psi_nxgrid, ct.psi_nygrid, ct.psi_nzgrid,
	    &xstart, &ystart, &zstart);


    /* If there is any overlap then we have to generate the mapping */
    if (map)
    {
	
	/*Starting index for ylm function: Indexing is as follows: 0:s, 1:px, 2:py, 3:pz, 4:dxx, etc.*/
	yindex = l*l + m;
	
	invdr = 1.0 / sp->drlig_awave;
	icount = 0;

	xc = xstart;
	for (ix = 0; ix < sp->adim_wave; ix++)
	{
	    yc = ystart;
	    for (iy = 0; iy < sp->adim_wave; iy++)
	    {
		zc = zstart;
		for (iz = 0; iz < sp->adim_wave; iz++)
		{
		    if (((Aix[ix] >= ilow) && (Aix[ix] <= ihi)) &&
			    ((Aiy[iy] >= jlow) && (Aiy[iy] <= jhi)) &&
			    ((Aiz[iz] >= klow) && (Aiz[iz] <= khi)))
		    {
			pvec[icount] =
			    PY0_GRID * PZ0_GRID * (Aix[ix] % PX0_GRID) +
			    PZ0_GRID * (Aiy[iy] % PY0_GRID) + (Aiz[iz] % PZ0_GRID);

			x[0] = xc - iptr->xtal[0];
			x[1] = yc - iptr->xtal[1];
			x[2] = zc - iptr->xtal[2];
			r = metric (x);

			to_cartesian(x, vector);

			if (r <= sp->aradius)
			    psi[pvec[icount]] += linint (&sp->awave_lig[awave_idx][0], r, invdr) * ylm(yindex, vector);
			    

			icount++;
		    }

		    zc += ct.hzgrid;

		}           /* end for */

		yc += ct.hygrid;

	    }               /* end for */

	    xc += ct.hxgrid;

	}                   /* end for */

    }                       /* end if */


    /* Integrate to see the total "charge density" of wave function */
    t2 = 0.0;

    for (idx = 0; idx < P0_BASIS; idx++)
	t2 += psi[idx] * psi[idx];

    t2 = ct.vel *  real_sum_all (t2, pct.img_comm);


    /* Release our memory */
    my_free (pvec);

}                               /* end init_nuc */

/******/
