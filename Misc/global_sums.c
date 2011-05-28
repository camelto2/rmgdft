/************************** SVN Revision Information **************************
 **    $Id$    **
******************************************************************************/

/****f* QMD-MGDFT/global_sums.c *****
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
 *   void global_sums (REAL *vect, int *length)
 *   Sums an array over all processors. For serial machines it just returns.
 * INPUTS
 *   vect: a vector to be sumed. Each processor has his own value
 *   length: length of the vector
 * OUTPUT
 *   vect: Each processor gets the sum.
 * PARENTS
 *   app_nl.c mg_eig_state.c nlforce_d.c nlforce_p.c
 *   nlforce_s.c rft.c scf.c subdiag_mpi.c symmetry.c write_avgd.c 
 *   write_avgv.c write_zstates.c
 * CHILDREN
 *   Mpi_allreduce is a MPI routine
 * SOURCE
 */


#include "main.h"


#if MPI

#if HYBRID_MODEL

#include <hybrid.h>
#include <pthread.h>
volatile REAL *global_sums_vector, *tvector;
volatile int global_sums_vector_state = 0;
pthread_mutex_t global_sums_vector_lock = PTHREAD_MUTEX_INITIALIZER;


void global_sums_threaded (REAL *vect, int *length, int tid)
{

  REAL *rptr, *rptr1;

  scf_barrier_wait();
  pthread_mutex_lock(&global_sums_vector_lock);
      if(global_sums_vector_state == 0) {
          my_malloc (global_sums_vector, *length * THREADS_PER_NODE, REAL);
          my_malloc (tvector, *length * THREADS_PER_NODE, REAL);
      }
      global_sums_vector_state = 1;
  pthread_mutex_unlock(&global_sums_vector_lock);

  // Wait until everyone gets here
  scf_barrier_wait();
  QMD_scopy(*length, vect, 1, &global_sums_vector[*length * tid], 1);
  scf_barrier_wait();

  pthread_mutex_lock(&global_sums_vector_lock);
      if(global_sums_vector_state == 1) {
          MPI_Allreduce(global_sums_vector, tvector, *length * THREADS_PER_NODE, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
          global_sums_vector_state = 0;
      }
  pthread_mutex_unlock(&global_sums_vector_lock);
  QMD_scopy(*length,  &tvector[*length * tid], 1, vect, 1);

  // Must wait until all threads have copied the data to vect before freeing memory
  scf_barrier_wait();

  pthread_mutex_lock(&global_sums_vector_lock);
      // ensures that the memory is only freed once
      if(tvector != NULL) {
          my_free(tvector);
          my_free(global_sums_vector);
          tvector = NULL;
          global_sums_vector = NULL;
      }
  pthread_mutex_unlock(&global_sums_vector_lock);

}

#endif




void global_sums (REAL * vect, int *length, MPI_Comm comm)
{
    int sizr, steps, blocks, newsize, tid;
    REAL *rptr, *rptr1;
    REAL rptr2[100];
#if MD_TIMERS
    REAL time0;

    time0 = my_crtc ();
#endif

#if HYBRID_MODEL
    tid = get_thread_tid();
    if(tid >= 0) {
        global_sums_threaded(vect, length, tid);
        return;
    }
#endif

    /* Check for small vector case and handle on stack */
    if (*length < 100)
    {
        sizr = *length;
        QMD_scopy (sizr, vect, 1, rptr2, 1);
       	MPI_Allreduce (rptr2, vect, sizr, MPI_DOUBLE, MPI_SUM, comm);

		
#if MD_TIMERS
        rmg_timings (GLOBAL_SUMS_TIME, my_crtc () - time0);
#endif
        return;
    }

    my_malloc (rptr, MAX_PWRK, REAL);
    newsize = MAX_PWRK;
    blocks = *length / newsize;
    sizr = (*length % newsize);

    rptr1 = vect;

    for (steps = 0; steps < blocks; steps++)
    {
        QMD_scopy (newsize, rptr1, 1, rptr, 1);
        MPI_Allreduce (rptr, rptr1, newsize, MPI_DOUBLE, MPI_SUM, comm);  

        rptr1 += newsize;
    }

    if (sizr)
    {
        QMD_scopy (sizr, rptr1, 1, rptr, 1);
        MPI_Allreduce (rptr, rptr1, sizr, MPI_DOUBLE, MPI_SUM, comm);
    }

    my_free (rptr);

#if MD_TIMERS
    rmg_timings (GLOBAL_SUMS_TIME, my_crtc () - time0);
#endif
}                               /* end global_sums */



#else



void global_sums (REAL * vect, int *length, MPI_Comm comm)
{
    return;
}

#endif


/******/
