/************************** SVN Revision Information **************************
 **    $Id$    **
******************************************************************************/

/****f* QMD-MGDFT/latgen.c *****
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
 *   void latgen (int *ibrav, rmg_double_t *celldm, rmg_double_t *A0I, rmg_double_t *A1I, rmg_double_t *A2I, 
 *                rmg_double_t *OMEGAI, int *flag)
 *   sets up the crystallographic vectors a0, a1, and a2.
 * INPUTS
 *   ibrav: bravais lattice type
 *   celldm:  see the table below
 *   flag: if it is true, A0I, A1I, A2I are cell related (0-1.0)
 * OUTPUT
 *   A0I, A1I, A2I: lattice vector
 *   OMEGAI:  volume of a unit cell
 * PARENTS
 *   init.c
 * CHILDREN
 *   cross_product.c
 * SEE ALSO
 *   ibrav and celldm are defined in the table below
 *  Lattice constants  = celldm(1)
 * 
 * -----------------------------------------------------------------------
 * 
 * point group bravais lattice    ibrav  celldm(2)-celldm(6)
 * .......................................................................
 * 432,<4>3m,m3m     sc          1     not used
 * .......................................................................
 * 23,m3             sc          1         "
 * .......................................................................
 * 432,<4>3m,m3m    fcc          2         "
 * .......................................................................
 * 23,m3            fcc          2         "
 * .......................................................................
 * 432,<4>3m,m3m    bcc          3         "
 * .......................................................................
 * 23,m3            bcc          3         "
 * .......................................................................
 * 622,6mm,                            
 * <6>m2,6/mmm      hex(p)       4      celldm(3)=c/a
 * .......................................................................
 * 6,<6>,6/m,       hex(p)
 * 32,3m,<3>m      trig(p)       4         "
 * .......................................................................
 * 3,<3>           trig(p)       4         "
 * .......................................................................
 * 32,3m,<3>m      trig(r)       5     celldm(4)=cos(aalpha)
 * .......................................................................
 * 3,<3>           trig(r)       5         "
 * .......................................................................
 * 422,4mm, 
 * <4>2m,4/mmm     tetr(p)       6      celldm(3)=c/a
 * .......................................................................
 * 4,<4>,4/m       tetr(p)       6         "
 * .......................................................................
 * 422,4mm,
 * <4>2m,4/mmm     tetr(i)       7         "
 * .......................................................................
 * 4,<4>,4/m       tetr(i)       7         "
 * .......................................................................
 * 222,mm2,mmm     orth(p)       8     above + celldm(2)=b/a
 * .......................................................................
 * 2,m,2/m         mcln(p)      12     above + celldm(4)=cos(ab)
 * .......................................................................
 * 1,<1>           tcln(p)      14     celldm(2)= b/a
 *                                     celldm(3)= c/a
 *				       celldm(4)= cos(bc)
 *	  			       celldm(5)= cos(ac)
 *				       celldm(6)= cos(ab)
 * -----------------------------------------------------------------------
 * SOURCE
 */


#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "grid.h"
#include "main.h"
#include "recips.h"


#define     SQRT2         1.414213562373
#define     SQRT3         1.732050807569


/* lengths of the sides of the supercell */
static rmg_double_t xside;
static rmg_double_t yside;
static rmg_double_t zside;

/* lattice vectors */
static rmg_double_t a0[3];
static rmg_double_t a1[3];
static rmg_double_t a2[3];

/** Total cell volume */
static rmg_double_t omega;


/** Global uniform grid spacing in x */
static rmg_double_t hxgrid;

/** Global uniform grid spacing in y */
static rmg_double_t hygrid;

/** Global uniform grid spacing in z */
static rmg_double_t hzgrid;

/** The fine uniform grid spacing in x */
static rmg_double_t hxxgrid;

/** The fine uniform grid spacing in y */
static rmg_double_t hyygrid;

/** The fine uniform grid spacing in z */
static rmg_double_t hzzgrid;

rmg_double_t get_xside(void)
{
    return xside;
}
rmg_double_t get_yside(void)
{
    return yside;
}
rmg_double_t get_zside(void)
{
    return zside;
}
rmg_double_t get_hxgrid(void)
{
    return hxgrid;
}
rmg_double_t get_hygrid(void)
{
    return hygrid;
}
rmg_double_t get_hzgrid(void)
{
    return hzgrid;
}

/* If flag is true then A0I,A1I,A2I are cell relative (0.0-1.0) */
void latgen (int *ibrav, rmg_double_t * celldm, rmg_double_t * A0I, rmg_double_t * A1I, rmg_double_t * A2I,
             rmg_double_t * OMEGAI, int *flag)
{

    int ir;
    rmg_double_t term, term1, term2, cbya, sine, singam;
    rmg_double_t distance, t1;
    rmg_double_t cvec[3];

    /* Initialise the appropriate variables */

    alat = celldm[0];

    for (ir = 0; ir < 3; ir++)
    {
        A0I[ir] = 0.0;
        A1I[ir] = 0.0;
        A2I[ir] = 0.0;
    }

    switch (*ibrav)
    {
    case CUBIC_PRIMITIVE:

        A0I[0] = celldm[0];
        A1I[1] = celldm[1];
        if (celldm[1] <= 0.0)
            A1I[1] = celldm[0];
        A2I[2] = celldm[2];
        if (celldm[2] <= 0.0)
            A2I[2] = celldm[0];
        break;

    case CUBIC_FC:

        term = alat / 2.0;
        A0I[0] = term;
        A0I[1] = term;
        A1I[1] = term;
        A1I[2] = term;
        A2I[0] = term;
        A2I[2] = term;
        break;

    case CUBIC_BC:

        term = alat / 2.0;
        for (ir = 0; ir < 3; ir++)
        {
            A0I[ir] = term;
            A1I[ir] = term;
            A2I[ir] = term;
        }                       /* for ir */
        A0I[2] = -term;
        A1I[0] = -term;
        A2I[1] = -term;
        break;

    case HEXAGONAL:
        cbya = celldm[2];
        A0I[0] = alat;
        A1I[0] = alat / 2.0;
        A1I[1] = alat * SQRT3 / 2.0;
        A2I[2] = alat * cbya;
        break;

    case TRIGONAL_PRIMITIVE:

        term1 = sqrt (1.0 + 2.0 * celldm[3]);
        term2 = sqrt (1.0 - celldm[3]);
        A0I[1] = SQRT2 * alat * term2 / SQRT3;
        A0I[2] = alat * term1 / SQRT3;
        A1I[0] = alat * term2 / SQRT2;
        A1I[1] = -A1I[0] / SQRT3;
        A1I[2] = A0I[2];
        A2I[0] = -A1I[0];
        A2I[1] = A1I[1];
        A2I[2] = A0I[2];
        break;

    case TETRAGONAL_PRIMITIVE:

        cbya = celldm[2];
        A0I[0] = alat;
        A1I[1] = alat;
        A2I[2] = alat * cbya;
        break;

    case TETRAGONAL_BC:

        cbya = celldm[2];
        A0I[0] = alat / 2.0;
        A0I[1] = A0I[0];
        A0I[2] = cbya * alat / 2.0;
        A1I[0] = A0I[0];
        A1I[1] = -A0I[0];
        A1I[2] = A0I[2];
        A2I[0] = -A0I[0];
        A2I[1] = -A0I[0];
        A2I[2] = A0I[2];
        break;

    case ORTHORHOMBIC_PRIMITIVE:

        A0I[0] = alat;
        A1I[1] = alat * celldm[1];
        A2I[2] = alat * celldm[2];
        break;

    case ORTHORHOMBIC_BASE_CENTRED:

        /* not programmed */
        error_handler ("bravais lattice not programmed.");
        break;

    case ORTHORHOMBIC_BC:

        /* not programmed */
        error_handler ("bravais lattice not programmed.");
        break;

    case ORTHORHOMBIC_FC:

        /* not programmed */
        error_handler ("bravais lattice not programmed.");
        break;

    case MONOCLINIC_PRIMITIVE:

        sine = sqrt (1.0 - celldm[3] * celldm[3]);
        A0I[0] = alat;
        A1I[0] = alat * celldm[1] * celldm[3];
        A1I[1] = alat * celldm[1] * sine;
        A2I[2] = alat * celldm[2];
        break;

    case MONOCLINIC_BASE_CENTRED:
        /* not programmed */
        error_handler ("bravais lattice not programmed.");
        break;

    case TRICLINIC_PRIMITIVE:

        singam = sqrt (1.0 - celldm[5] * celldm[5]);
        term = sqrt ((1.0 + 2.0 * celldm[3] * celldm[4] * celldm[5] -
                      celldm[3] * celldm[3]
                      - celldm[4] * celldm[4]
                      - celldm[5] * celldm[5]) / (1.0 - celldm[5] * celldm[5]));
        A0I[0] = alat;
        A1I[0] = alat * celldm[1] * celldm[5];
        A1I[1] = alat * celldm[1] * singam;
        A2I[0] = alat * celldm[2] * celldm[4];
        A2I[1] = alat * celldm[2] * (celldm[3] - celldm[4] * celldm[5]) / singam;
        A2I[2] = alat * celldm[2] * term;

        break;

    default:

        Dprintf ("ct.ibrav is set to %d", ct.ibrav);
        error_handler ("bravais lattice not programmed.");


    }                           /* end switch (*ibrav) */


    cross_product (A0I, A1I, cvec);
    *OMEGAI = cvec[0] * A2I[0] + cvec[1] * A2I[1] + cvec[2] * A2I[2];

    *OMEGAI = fabs (*OMEGAI);

    /* Generate volume element */
    t1 = (rmg_double_t) (ct.psi_nbasis);
    ct.vel = *OMEGAI / t1;

    t1 = (rmg_double_t) (ct.psi_fnbasis);
    ct.vel_f = *OMEGAI / t1;

    /* Calculate length of supercell */
    distance = 0.0;
    for (ir = 0; ir < 3; ir++)
        distance += A0I[ir] * A0I[ir];

    xside = sqrt (distance);

    distance = 0.0;
    for (ir = 0; ir < 3; ir++)
        distance += A1I[ir] * A1I[ir];

    yside = sqrt (distance);

    distance = 0.0;
    for (ir = 0; ir < 3; ir++)
        distance += A2I[ir] * A2I[ir];

    zside = sqrt (distance);

    /* Calculate grid size in crystal coordinates */

    t1 = (rmg_double_t) ct.psi_nxgrid;
    hxgrid = 1.0 / t1;
    ct.hxxgrid = hxgrid / (rmg_double_t) FG_NX;

    t1 = (rmg_double_t) ct.psi_nygrid;
    hygrid = 1.0 / t1;
    ct.hyygrid = hygrid / (rmg_double_t) FG_NY;

    t1 = (rmg_double_t) ct.psi_nzgrid;
    hzgrid = 1.0 / t1;
    ct.hzzgrid = hzgrid / (rmg_double_t) FG_NZ;

    if (*flag)
    {
        for (ir = 0; ir < 3; ir++)
        {
            A0I[ir] /= celldm[0];
            A1I[ir] /= celldm[0];
            A2I[ir] /= celldm[0];
        }                       /* end for */
    }                           /* end if */

}                               /* end latgen */

/******/
