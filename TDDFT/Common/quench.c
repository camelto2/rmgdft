/************************** SVN Revision Information **************************
 **    $Id$    **
 ******************************************************************************/

/****f* QMD-MGDFT/quench.c *****
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
 *   void quench(STATE *states, REAL *vxc, REAL *vh, REAL *vnuc, 
 *               REAL *rho, REAL *rhocore, REAL *rhoc)
 *   For a fixed atomic configuration, quench the electrons to find 
 *   the minimum total energy 
 * INPUTS
 *   states: point to orbital structure (see main.h)
 *   vxc:    exchange correlation potential
 *   vh:     Hartree potential
 *   vnuc:   Pseudopotential 
 *   rho:    total valence charge density
 *   rhocore: core chare density only for non-linear core correction
 *   rhoc:   Gaussian compensating charge density
 * OUTPUT
 *   states, vxc, vh, rho are updated
 * PARENTS
 *   cdfastrlx.c fastrlx.c md.c
 * CHILDREN
 *   scf.c force.c get_te.c subdiag.c get_ke.c
 * SOURCE
 */

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "main.h"

void update_pot(double *, double *, double *, double *, double *, double *,
        double *, double *, int *, STATE * states);
void pulay_rho (int step0, int N, double *xm, double *fm, int NsavedSteps,
        int Nrefresh, double scale, int preconditioning);
static double t[2];
extern int it_scf;
double tem1;

void quench(STATE * states, STATE * states1, REAL * vxc, REAL * vh,
        REAL * vnuc, REAL * vh_old, REAL * vxc_old, REAL * rho, REAL * rhoc, REAL * rhocore)
{
    int outcount = 0;
    int state_plot, i;
    static int CONVERGENCE = FALSE;
    double tem, time1, time2;
    int idx, ione =1;

    time1 = my_crtc();


    for (ct.scf_steps = 0; ct.scf_steps < ct.max_scf_steps; ct.scf_steps++)
    {
        if (pct.gridpe == 0)
            printf("\n\n\n ITERATION     %d\n", ct.scf_steps);

        matrix_and_diag(ct.kp[0].kstate, states1, vtot_c, 0);
        /* Generate new density */
        ct.efermi = fill(states, ct.occ_width, ct.nel, ct.occ_mix, ct.num_states, ct.occ_flag);

        if (pct.gridpe == 0 && ct.occ_flag == 1)
            printf("FERMI ENERGY = %15.8f\n", ct.efermi * Ha_eV);

        scopy(&FP0_BASIS, rho, &ione, rho_old, &ione);

        get_new_rho(states, rho);

        tem1 = 0.0;
        for (idx = 0; idx < FP0_BASIS; idx++)
        {
            tem = rho_old[idx];
            rho_old[idx] = -rho[idx] + rho_old[idx];
            rho[idx] = tem;
            tem1 += rho_old[idx] * rho_old[idx];
        }

        tem1 = sqrt(real_sum_all (tem1, pct.grid_comm) ) /(double) FP0_BASIS;
        pulay_rho (ct.scf_steps, FP0_BASIS, rho, rho_old, ct.charge_pulay_order, ct.charge_pulay_refresh, ct.mix, 0); 


        /* Update potential */
        update_pot(vxc, vh, vxc_old, vh_old, vnuc, rho, rhoc, rhocore, &CONVERGENCE, states);


        get_te(rho, rhoc, rhocore, vh, vxc, states);


        time2 = my_crtc();
        rmg_timings(SCF_TIME, time2 - time1);

    }                               /* end scf */


}

/*
   Function to update potentials vh and vxc:

   The new potentials are computed as a linear combination 
   of the old ones (input "vh" and "vxc") and the ones 
   corresponding to the input "rho".
   */
void update_pot(double *vxc, double *vh, REAL * vxc_old, REAL * vh_old,
        double *vnuc, double *rho, double *rhoc, double *rhocore,
        int *CONVERGENCE, STATE * states)
{
    int n = FP0_BASIS, idx, ione = 1;
    double tem;

    /* save old vtot, vxc, vh */
    scopy(&n, vxc, &ione, vxc_old, &ione);
    scopy(&n, vh, &ione, vh_old, &ione);

    for (idx = 0; idx < FP0_BASIS; idx++)
        vtot[idx] = vxc[idx] + vh[idx];

    /* Generate exchange-correlation potential */
    get_vxc(rho, rhocore, vxc);

    pack_vhstod(vh, ct.vh_ext, FPX0_GRID, FPY0_GRID, FPZ0_GRID);

    /* Keep in memory vh*rho_new before updating vh */
    tem = ddot(&FP0_BASIS, rho, &ione, vh, &ione);
    ct.Evhold_rho = 0.5 * ct.vel_f * real_sum_all(tem, pct.grid_comm);


    /* Generate hartree potential */
    //    get_vh1(rho, rhoc, vh, 15, ct.poi_parm.levels);
    get_vh (rho, rhoc, vh, ct.hartree_min_sweeps, ct.hartree_max_sweeps, ct.poi_parm.levels, ct.rms/ct.hartree_rms_ratio);



    /* Compute quantities function of rho only */
    tem = ddot(&FP0_BASIS, rho, &ione, vh, &ione);
    ct.Evh_rho = 0.5 * ct.vel_f * real_sum_all(tem, pct.grid_comm);

    tem = ddot(&FP0_BASIS, rhoc, &ione, vh, &ione);
    ct.Evh_rhoc = 0.5 * ct.vel_f * real_sum_all(tem, pct.grid_comm);



    /* evaluate correction vh+vxc */
    for (idx = 0; idx < FP0_BASIS; idx++)
        vtot[idx] = vxc[idx] + vh[idx] - vtot[idx];

    /* evaluate SC correction */
    t[0] = t[1] = 0.;

    for (idx = 0; idx < FP0_BASIS; idx++)
    {
        t[0] += rho[idx] * vtot[idx];
        t[1] += vtot[idx] * vtot[idx];
    }
    idx = 2;
    global_sums(t, &idx, pct.grid_comm);
    t[0] *= ct.vel_f;
    t[0] /= (double) ct.num_ions;
    t[1] = sqrt(t[1] / ((double) (ct.vh_nbasis)));

    ct.rms = t[1];
    if (pct.gridpe == 0)
        printf(" SCF CHECKS: RMS[dv] = %15.10e RMS[drho] = %15.10e \n", t[1], tem1);

    fflush(NULL);
    my_barrier();

    if (ct.scf_steps < 4 && ct.runflag == 0)
    {
        for (idx = 0; idx < FP0_BASIS; idx++)
        {
            vxc[idx] = vxc_old[idx];
            vh[idx] = vh_old[idx];
        }
    }

    for (idx = 0; idx < FP0_BASIS; idx++)
        vtot[idx] = vxc[idx] + vh[idx] + vnuc[idx];

    get_vtot_psi(vtot_c, vtot, FG_NX);

    if (t[1] < ct.thr_rms)
        *CONVERGENCE = TRUE;
}

