/************************** SVN Revision Information **************************
 **    $Id$    **
******************************************************************************/

/****f* QMD-MGDFT/main.c *****
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
 *   int main(int argc, char **argv)
 *   Main program
 *   Perform any initializations that are required and then
 *   enters the main driver loop. It also handles checkpointing and the
 *   output of intermediate results.
 * INPUTS
 *   when we run it, we need to give the input control file name in the first argument
 *   for example, md in.diamond8
 * OUTPUT
 *   Print standard output.
 * PARENTS
 *   This is grand-grand-....
 * CHILDREN
 *   init_pe.c write_header.c write_occ.c quench.c fastrlx.c cdfastrlx.c moldyn.c
 *   dendx.c psidx.c write_data.c write_avgv.c write_avgd.c write_zstates.c get_milliken.c
 * SEE ALSO
 *   main.h for structure definition
 * SOURCE
 */


#include <float.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "../Headers/main.h"
#include "../Headers/common_prototypes.h"

void initialize (int argc, char **argv);

void run (void);

void report (void);

void finish (void);

/* Global MPI stuff. Overridden by input params */
int NPES=1;
int PE_X=1;
int PE_Y=1;
int PE_Z=1;

/* Global coarse grid dimesions. Set by input file */
int NX_GRID;
int NY_GRID;
int NZ_GRID;

/* Global fine grid dimensions */
int FNX_GRID;
int FNY_GRID;
int FNZ_GRID;

/* Fine/coarse grid ratios */
int FG_NX;
int FG_NY;
int FG_NZ;

/* State storage pointer (memory allocated dynamically in rd_cont */
STATE *states;



/* Electronic charge density or charge density of own spin in polarized case */
rmg_double_t *rho;

/*  Electronic charge density of pposite spin density*/
rmg_double_t *rho_oppo;  


/* Core Charge density */
rmg_double_t *rhocore;


/* Compensating charge density */
rmg_double_t *rhoc;


/* Hartree potential */
rmg_double_t *vh;

/* Nuclear local potential */
rmg_double_t *vnuc;

/* Exchange-correlation potential */
rmg_double_t *vxc;


/* Main control structure which is declared extern in main.h so any module */
/* may access it.					                 */
CONTROL ct;

/* PE control structure which is also declared extern in main.h */
PE_CONTROL pct;

/*Other global variables will be defined here, so that they can be defined
 * in header files as extern as it should be. Otherwise compiler
 * on Origin complains that the variables are multiply defined*/

/*Variables from recips.h*/
double b0[3], b1[3], b2[3];
double alat;


int main (int argc, char **argv)
{

    char *tptr;

#if GPU_ENABLED
//  Hack to force initialization of libsci on Cray before we create our own threads
    char *trans = "n";
    int asize = 32, i, j;
    rmg_double_t alpha = 1.0;
    rmg_double_t beta = 0.0;
    rmg_double_t A[32*32], B[32*32], C[32*32];

    for(i = 0;i < asize * asize;i++) {
        A[i] = 1.0;
        B[i] = 0.0;
        C[i] = 1.0;
    }


    dgemm (trans, trans, &asize, &asize, &asize, &alpha, A, &asize,
               B, &asize, &beta, C, &asize);
#endif

    // Get RMG_IMAGES_PER_NODE environment variable
    ct.images_per_node = 1;
    if(NULL != (tptr = getenv("RMG_IMAGES_PER_NODE"))) {
        ct.images_per_node = atoi(tptr);
    }

    // Get RMG_MPI_THREAD_LEVEL environment variable
    ct.mpi_threadlevel = MPI_THREAD_SERIALIZED;
    if(NULL != (tptr = getenv("RMG_MPI_THREAD_LEVEL"))) {
        ct.mpi_threadlevel = atoi(tptr);
    }

    initialize (argc, argv);

    run ();

    report ();

    finish ();

    return 0;
}


void initialize(int argc, char **argv) 
{

    int FP0_BASIS;

    /* start the benchmark clock */
    ct.time0 = my_crtc ();

    /* Initialize all I/O including MPI group comms */
    /* Also reads control and pseudopotential files*/
    init_IO (argc, argv);

    FP0_BASIS = get_FP0_BASIS();

    int num_images = pct.images;
    num_images = 1;
    lbfgs_init(ct.num_ions, num_images);

    my_malloc (rho, FP0_BASIS, rmg_double_t);
    my_malloc (rhocore, FP0_BASIS, rmg_double_t);
    my_malloc (rhoc, FP0_BASIS, rmg_double_t);
    my_malloc (vh, FP0_BASIS, rmg_double_t);
    my_malloc (vnuc, FP0_BASIS, rmg_double_t);
    my_malloc (vxc, FP0_BASIS, rmg_double_t);

    /* for spin polarized calculation, allocate memory for density of the opposite spin */
    if(ct.spin_flag)
    	    my_malloc (rho_oppo, FP0_BASIS, rmg_double_t);


    /* initialize states */
    states = init_states (); 


    my_barrier ();

    /* Record the rime it took from the start of run until we hit init */
    rmg_timings ( PREINIT_TIME, my_crtc () - ct.time0);

    /* Perform any necessary initializations */
    init (vh, rho, rho_oppo, rhocore, rhoc, states, vnuc, vxc);



   /* Need if statement here, otherwise job output file 
    * will also show information of control file ? */
   if (pct.imgpe == 0)
   {
    
    /* Write header to stdout */
    write_header (); 

   }


    /* Write state occupations to stdout */
    write_occ (states); 

    
    /* Flush the results immediately */
    fflush (NULL);



    /* Wait until everybody gets here */
    /* MPI_Barrier(MPI_COMM_WORLD); */
    MPI_Barrier(pct.img_comm);

}

void run (void)
{

    rmg_double_t time2;

    /* Dispatch to the correct driver routine */
    switch (ct.forceflag)
    {

    case MD_QUENCH:            /* Quench the electrons */
        relax (0, states, vxc, vh, vnuc, rho, rho_oppo, rhocore, rhoc);
        break;

    case MD_FASTRLX:           /* Fast relax */
        relax (ct.max_md_steps, states, vxc, vh, vnuc, rho, rho_oppo, rhocore, rhoc);
        break;

    case NEB_RELAX:           /* nudged elastic band relax */
        neb_relax (states, vxc, vh, vnuc, rho, rho_oppo, rhocore, rhoc);
        break;

    case MD_CVE:               /* molecular dynamics */
    case MD_CVT:
    case MD_CPT:
        quench (states, vxc, vh, vnuc, rho, rho_oppo, rhocore, rhoc);
        moldyn (states, vxc, vh, vnuc, rho, rho_oppo, rhoc, rhocore);
        break;

    case BAND_STRUCTURE:
        bandstructure (states, vxc, vh, vnuc);
        break;
    default:
        error_handler ("Undefined MD method");

    }

    time2 = my_crtc ();

    rmg_timings (FINISH_TIME, (my_crtc () - time2));

}                               /* end run */

void report ()
{

    /* write planar averages of quantities */
    if (ct.zaverage == 1)
    {
        /* output the average potential */
        write_avgv (vh, vnuc);
        write_avgd (rho);
    }
    else if (ct.zaverage == 2)
    {
        write_zstates (states);
    }


    /* If milliken population info is requested then compute and output it */
    /*if (ct.domilliken)
        mulliken (states);*/


    /*Destroy wisdom that may have been allocated previously */
    destroy_fftw_wisdom ();


    if (ct.write_memory_report)
    {
        /*Release memory for projectors first */
        finish_release_mem (states);

    }


    /* Release the memory for density of opposite spin */
    if (ct.spin_flag)
    	my_free (rho_oppo);

    /* Write timing information */
    write_timings ();
   
}                               /* end report */


void finish ()
{

	/*Exit Scalapack */
    if (pct.scalapack_pe)
        sl_exit (pct.ictxt);

	/*Exit MPI */
    MPI_Finalize ();

#if GPU_ENABLED
    finalize_gpu();
#endif

}                               /* end finish */


/******/
