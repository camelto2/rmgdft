/************************** SVN Revision Information **************************
 **    $Id$    **
******************************************************************************/

/****f* QMD-MGDFT/get_index.c *****
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
 *   int claim_ion (int gridpe, ION * iptr,  int pxgrid, int pygrid, int pzgrid, int nxgrid, int nygrid, int nzgrid)
 *   This function determined which processor owns an ion. This depends on its spatial position relative
 *   to regions belonging to processors. Obviosuly, each ion has to belong to some processor and no two (or more)
 *   processors can share an ion. The function returns rank of the owner.
 *
 * SOURCE
 */


#include "main.h"
#include <math.h>


int claim_ion (REAL *xtal,  int pxgrid, int pygrid, int pzgrid, int nxgrid, int nygrid, int nzgrid)
{

    int  pe;
    int ii, jj, kk, ilow, ihi, klow, khi, jlow, jhi;
    int igridx, igridy, igridz;
    REAL t1, t2;



    /*Figure out grid coordinates of a grid point closest to position of ion
     * under consideration*/
    t1 = (xtal[0]) * (REAL) nxgrid;
    t1 = modf (t1, &t2);
    igridx = (int) t2;
    if (t1 > 0.5)
        igridx++;
    
    if (igridx >= nxgrid) igridx -= nxgrid ;
    
    t1 = (xtal[1]) * (REAL) nygrid;
    t1 = modf (t1, &t2);
    igridy = (int) t2;
    if (t1 > 0.5)
        igridy++;
    
    if (igridy >= nygrid) igridy -= nygrid ;
    
    t1 = (xtal[2]) * (REAL) nzgrid;
    t1 = modf (t1, &t2);
    igridz = (int) t2;
    if (t1 > 0.5)
        igridz++;
    
    if (igridz >= nzgrid) igridz -= nzgrid ;

    /*Now find the rank of the owner*/
//    pe = (igridx / pxgrid) * PE_Y * PE_Z + (igridy / pygrid) * PE_Z + (igridz / pzgrid);
    pe = find_grid_1d_owner(igridx, nxgrid, PE_X) * PE_Y * PE_Z +
         find_grid_1d_owner(igridy, nygrid, PE_Y) * PE_Z +
         find_grid_1d_owner(igridz, nzgrid, PE_Z);

    return (pe);

}                               /* end get_index */

/******/
