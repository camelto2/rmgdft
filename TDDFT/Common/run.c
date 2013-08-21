/************************** SVN Revision Information **************************
 **    $Id$    **
******************************************************************************/
 
/****f* QMD-MGDFT/run.c *****
 * NAME
 *   Ab initio O(n) real space code with 
 *   localized orbitals and multigrid acceleration
 *   Version: 3.0.0
 * COPYRIGHT
 *   Copyright (C) 2001  Wenchang Lu,
 *                       Jerzy Bernholc
 * FUNCTION
 *   void run()   
 *   Perform any initializations that are required and then
 *   enters the main driver loop. It also handles checkpointing and the
 *   output of intermediate results.
 * INPUTS
 *   nothing
 * OUTPUT
 *   Print standard output.
 * PARENTS
 *   md.c
 * CHILDREN
 *   
 * SEE ALSO
 *   main.h for structure defination
 * SOURCE
 */


#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "main.h"

extern REAL *vh_old, *vxc_old;


void run(STATE * states, STATE * states1)
{
    REAL time1;
    int MAT_TRANSFER = 0;


    time1 = my_crtc();

    /* initialize processor structure, decompose the processor into subdomains
       pct.pe_kpoint * ( pct.pe_x, pct.pe_y, pct.pe_z) or
       pct.pe_kpoint * ( pct.pe_column , pct.pe_row)
     */
    init_dimension();
    init_pe_on();


    if (pct.gridpe == 0)
        printf("\n  MXLLDA: %d ", MXLLDA);

    /* allocate memory for matrixs  */
    allocate_matrix();

    /* Perform some necessary initializations no matter localized or not  
     */
    my_malloc_init( vxc_old, FP0_BASIS, REAL );
    my_malloc_init( vh_old, FP0_BASIS, REAL );

    init(vh, rho, rhocore, rhoc, states, states1, vnuc, vxc, vh_old, vxc_old);

    my_barrier();

    /* Dispatch to the correct driver routine */

        quench(states, states1, vxc, vh, vnuc, vh_old, vxc_old, rho, rhoc, rhocore);

    /* Save data to output file */
    write_data(ct.outfile, vh, vxc, vh_old, vxc_old, rho, &states[0]); 


    my_barrier();

    time1 = my_crtc() - time1;
    rmg_timings(TOTAL_TIME, time1);

}
