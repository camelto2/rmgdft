/************************** SVN Revision Information **************************
 **    $Id: app_del2c.c 1407 2011-05-16 16:40:39Z luw $    **
******************************************************************************/

/****f* QMD-MGDFT/app_del2c.c *****
 * NAME
 *   Ab initio real space code with multigrid acceleration
 *   Quantum molecular dynamics package.
 *   Version: 2.1.5
 * COPYRIGHT
 *   Copyright (C) 1995  Emil Briggs
 *   Copyright (C) 1998  Emil Briggs, Charles Brabec, Mark Wensell, 
 *                       Dan Sullivan, Chris Rapcewicz, Jerzy Bernholc
 *   Copyright (C) 2001  Emil Briggs, Wenchang Lu,
 *                       Marco Buongiorno Nardelli,Charles Brabec, 
 *                       Mark Wensell,Dan Sullivan, Chris Rapcewicz,
 *                       Jerzy Bernholc
 * FUNCTION
 *    rmg_float_t app_del2c(rmg_float_t *a, rmg_float_t *b, int dimx, int dimy, int dimz,
 *                   rmg_float_t gridhx, rmg_float_t gridhy, rmg_float_t gridhz)
 *    Applies a 7-point finite difference discretization of the 
 *    Laplacian operator to  a matrix 
 * INPUTS
 *    a[(dimx+2)*(dimy+2)*(dimz+2)]: the matrix the Laplacian is applied to
 *    dimx, dimy, dimz: array dimentions in x, y, z direction
 *    gridhx, gridhy, gridhz:  grid spacings in crystal coordinates
 *
 *    Note: The Laplacian is not applied to the boundary layer but the 
 *          boundary layer must contain the correct values before entrance 
 *          into the function.
 * OUTPUT
 *    b[(dimx+2)*(dimy+2)*(dimz+2)]: the results of Laplacian(a)
 * PARENTS
 *    solv_pois.c, eval_residual.c
 * CHILDREN
 *    nothing
 * SOURCE
 */


#include "const.h"
#include "common_prototypes.h"
#include "fixed_dims.h"
#include "rmg_alloc.h"

#include <float.h>
#include <math.h>


rmg_double_t app_del2c_f (rmg_float_t * a, rmg_float_t * b, int dimx, int dimy, int dimz,
                rmg_double_t gridhx, rmg_double_t gridhy, rmg_double_t gridhz)
{

    int ix, iy, iz, ibrav;
    int incy, incx;
    rmg_double_t cc = 0.0, fcx, fcy, fcz, fc, fc1, fc2;
    int ixs, iys, ixms, ixps, iyms, iyps;


    ibrav = get_ibrav_type();

    incy = dimz + 2;
    incx = (dimz + 2) * (dimy + 2);

    switch (ibrav)
    {

    case CUBIC_PRIMITIVE:
    case ORTHORHOMBIC_PRIMITIVE:

        if (get_anisotropy() < 1.0000001)
        {

            cc = -2.0 / (gridhx * gridhx * get_xside() * get_xside());
            cc = cc - 2.0 / (gridhy * gridhy * get_yside() * get_yside());
            cc = cc - 2.0 / (gridhz * gridhz * get_zside() * get_zside());
            fcx = 1.0 / (gridhx * gridhx * get_xside() * get_xside());

            for (ix = 1; ix <= dimx; ix++)
            {
                ixs = ix * incx;
                ixms = (ix - 1) * incx;
                ixps = (ix + 1) * incx;

                if (dimy % 2)
                {

                    for (iy = 1; iy <= dimy; iy++)
                    {
                        iys = iy * incy;
                        iyms = (iy - 1) * incy;
                        iyps = (iy + 1) * incy;

                        for (iz = 1; iz <= dimz; iz++)
                        {

                            b[ixs + iys + iz] =
                                cc * a[ixs + iys + iz] +
                                fcx * (a[ixms + iys + iz] +
                                       a[ixps + iys + iz] +
                                       a[ixs + iyms + iz] +
                                       a[ixs + iyps + iz] +
                                       a[ixs + iys + (iz - 1)] + a[ixs + iys + (iz + 1)]);


                        }       /* end for */

                    }           /* end for */

                }
                else
                {

                    for (iy = 1; iy <= dimy; iy += 2)
                    {
                        iys = iy * incy;
                        iyms = (iy - 1) * incy;
                        iyps = (iy + 1) * incy;

                        for (iz = 1; iz <= dimz; iz++)
                        {

                            b[ixs + iys + iz] =
                                cc * a[ixs + iys + iz] +
                                fcx * (a[ixs + iys + (iz - 1)] +
                                       a[ixs + iys + (iz + 1)] +
                                       a[ixms + iys + iz] +
                                       a[ixps + iys + iz] +
                                       a[ixs + iyms + iz] + a[ixs + iyps + iz]);

                            b[ixs + iyps + iz] =
                                cc * a[ixs + iyps + iz] +
                                fcx * (a[ixs + iyps + (iz - 1)] +
                                       a[ixs + iyps + (iz + 1)] +
                                       a[ixms + iyps + iz] +
                                       a[ixps + iyps + iz] +
                                       a[ixs + iys + iz] + a[ixs + iyps + incy + iz]);

                        }       /* end for */

                    }           /* end for */

                }               /* end if */

            }                   /* end for */

        }
        else
        {

            cc = -2.0 / (gridhx * gridhx * get_xside() * get_xside());
            cc = cc - 2.0 / (gridhy * gridhy * get_yside() * get_yside());
            cc = cc - 2.0 / (gridhz * gridhz * get_zside() * get_zside());
            fcx = 1.0 / (gridhx * gridhx * get_xside() * get_xside());
            fcy = 1.0 / (gridhy * gridhy * get_yside() * get_yside());
            fcz = 1.0 / (gridhz * gridhz * get_zside() * get_zside());

            for (ix = 1; ix <= dimx; ix++)
            {
                ixs = ix * incx;
                ixms = (ix - 1) * incx;
                ixps = (ix + 1) * incx;

                for (iy = 1; iy <= dimy; iy++)
                {
                    iys = iy * incy;
                    iyms = (iy - 1) * incy;
                    iyps = (iy + 1) * incy;

                    for (iz = 1; iz <= dimz; iz++)
                    {

                        b[ixs + iys + iz] =
                            cc * a[ixs + iys + iz] +
                            fcx * a[ixms + iys + iz] +
                            fcx * a[ixps + iys + iz] +
                            fcy * a[ixs + iyms + iz] +
                            fcy * a[ixs + iyps + iz] +
                            fcz * a[ixs + iys + (iz - 1)] + fcz * a[ixs + iys + (iz + 1)];


                    }           /* end for */

                }               /* end for */

            }                   /* end for */

        }                       /* end if */

        break;

    case CUBIC_BC:

        cc = -2.0 / (gridhx * gridhx * get_xside() * get_xside());
        fc = 1.0 / (4.0 * gridhx * gridhx * get_xside() * get_xside());

        for (ix = 1; ix <= dimx; ix++)
        {
            ixs = ix * incx;
            ixms = (ix - 1) * incx;
            ixps = (ix + 1) * incx;

            for (iy = 1; iy <= dimy; iy++)
            {
                iys = iy * incy;
                iyms = (iy - 1) * incy;
                iyps = (iy + 1) * incy;

                for (iz = 1; iz <= dimz; iz++)
                {

                    b[ixs + iys + iz] =
                        cc * a[ixs + iys + iz] +
                        fc * a[ixms + iyms + iz - 1] +
                        fc * a[ixms + iys + iz] +
                        fc * a[ixs + iyms + iz] +
                        fc * a[ixs + iys + iz - 1] +
                        fc * a[ixs + iys + iz + 1] +
                        fc * a[ixs + iyps + iz] +
                        fc * a[ixps + iys + iz] + fc * a[ixps + iyps + iz + 1];


                }               /* end for */

            }                   /* end for */

        }                       /* end for */
        break;

    case CUBIC_FC:

        cc = -6.0 / (gridhx * gridhx * get_xside() * get_xside());
        fc = 1.0 / (2.0 * gridhx * gridhx * get_xside() * get_xside());

        for (ix = 1; ix <= dimx; ix++)
        {
            ixs = ix * incx;
            ixms = (ix - 1) * incx;
            ixps = (ix + 1) * incx;

            for (iy = 1; iy <= dimy; iy++)
            {
                iys = iy * incy;
                iyms = (iy - 1) * incy;
                iyps = (iy + 1) * incy;

                for (iz = 1; iz <= dimz; iz++)
                {

                    b[ixs + iys + iz] =
                        cc * a[ixs + iys + iz] +
                        fc * a[ixms + iys + iz] +
                        fc * a[ixms + iys + iz + 1] +
                        fc * a[ixms + iyps + iz] +
                        fc * a[ixs + iyms + iz] +
                        fc * a[ixs + iyms + iz + 1] +
                        fc * a[ixs + iys + iz - 1] +
                        fc * a[ixs + iys + iz + 1] +
                        fc * a[ixs + iyps + iz - 1] +
                        fc * a[ixs + iyps + iz] +
                        fc * a[ixps + iyms + iz] +
                        fc * a[ixps + iys + iz - 1] + fc * a[ixps + iys + iz];


                }               /* end for */

            }                   /* end for */

        }                       /* end for */
        break;

    case HEXAGONAL:

        cc = -4.0 / (gridhx * gridhx * get_xside() * get_xside());
        cc = cc - 2.0 / (gridhz * gridhz * get_zside() * get_zside());
        fc1 = 2.0 / (3.0 * gridhx * gridhx * get_xside() * get_xside());
        fc2 = 1.0 / (gridhz * gridhz * get_zside() * get_zside());

        for (ix = 1; ix <= dimx; ix++)
        {
            ixs = ix * incx;
            ixms = (ix - 1) * incx;
            ixps = (ix + 1) * incx;

            for (iy = 1; iy <= dimy; iy++)
            {
                iys = iy * incy;
                iyms = (iy - 1) * incy;
                iyps = (iy + 1) * incy;

                for (iz = 1; iz <= dimz; iz++)
                {

                    b[ixs + iys + iz] =
                        cc * a[ixs + iys + iz] +
                        fc1 * a[ixps + iys + iz] +
                        fc1 * a[ixps + iyms + iz] +
                        fc1 * a[ixs + iyms + iz] +
                        fc1 * a[ixms + iys + iz] +
                        fc1 * a[ixms + iyps + iz] +
                        fc1 * a[ixs + iyps + iz] +
                        fc2 * a[ixs + iys + iz + 1] + fc2 * a[ixs + iys + iz - 1];


                }               /* end for */

            }                   /* end for */

        }                       /* end for */
        break;

    default:
        error_handler ("Lattice type not implemented");

    }                           /* end switch */


    /* Return the diagonal component of the operator */
    return cc;

}                               /* end app_del2c */


/******/
