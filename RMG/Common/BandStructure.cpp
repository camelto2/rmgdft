#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "transition.h"
#include "const.h"
#include "State.h"
#include "Kpoint.h"
#include "BaseThread.h"
#include "TradeImages.h"
#include "RmgTimer.h"
#include "RmgThread.h"
#include "RmgException.h"
#include "GlobalSums.h"
#include "rmgthreads.h"
#include "vhartree.h"
#include "packfuncs.h"
#include "typedefs.h"
#include "common_prototypes.h"
#include "common_prototypes1.h"
#include "rmg_error.h"
#include "Kpoint.h"
#include "Subdiag.h"
#include "../Headers/prototypes.h"

template void BandStructure(Kpoint<double> **, double *vh, double *vxc, double *vnuc);
template void BandStructure(Kpoint<std::complex<double> > **, double *vh, double *vxc, double *vnuc);

template <typename KpointType>
void BandStructure(Kpoint<KpointType> ** Kptr, double *vh, double *vxc, double *vnuc)
{
    
    double *vtot, *vtot_psi;
    double max_res;
    int FP0_BASIS, P0_BASIS;
    
    int idx, istop, st1, ist;

    bool CONVERGED;
    BaseThread *T = BaseThread::getBaseThread(0);


    
    P0_BASIS =  Rmg_G->get_P0_BASIS(1);
    FP0_BASIS = Rmg_G->get_P0_BASIS(Rmg_G->default_FG_RATIO);

    vtot = new  double[FP0_BASIS];
    vtot_psi = new double[P0_BASIS];

    for (idx = 0; idx < get_FP0_BASIS(); idx++)
        vtot[idx] = vxc[idx] + vh[idx] + vnuc[idx];

    get_ddd (vtot);
    get_vtot_psi (vtot_psi, vtot, get_FG_RATIO());


    // Loop over k-points
    for(int kpt = 0;kpt < ct.num_kpts;kpt++) {


        Betaxpsi (Kptr[kpt]);
        AppNls(Kptr[kpt], Kptr[kpt]->newsint_local);

        for (ct.scf_steps = 0, CONVERGED = false;
                ct.scf_steps < ct.max_scf_steps && !CONVERGED; ct.scf_steps++)
        {


            Subdiag (Kptr[kpt], vh, vnuc, vxc, ct.subdiag_driver);
            for(int vcycle = 0;vcycle < ct.eig_parm.mucycles;vcycle++) {
                Betaxpsi (Kptr[kpt]);
                Kptr[kpt]->mix_betaxpsi(0);
                AppNls(Kptr[kpt], Kptr[kpt]->oldsint_local);

                // Update betaxpsi        
                Betaxpsi (Kptr[kpt]);

                AppNls(Kptr[kpt], Kptr[kpt]->oldsint_local);
                //            Kptr[kpt]->mix_betaxpsi(0);

                /* Update the wavefunctions */
                istop = Kptr[kpt]->nstates / T->get_threads_per_node();
                istop = istop * T->get_threads_per_node();

                for(st1=0;st1 < istop;st1+=T->get_threads_per_node()) {
                    SCF_THREAD_CONTROL thread_control[MAX_RMG_THREADS];
                    for(ist = 0;ist < T->get_threads_per_node();ist++) {
                        thread_control[ist].job = HYBRID_EIG;
                        thread_control[ist].vtot = vtot_psi;
                        thread_control[ist].sp = &Kptr[kpt]->Kstates[st1 + ist];
                        thread_control[ist].p3 = (void *)Kptr[kpt];
                        T->set_pptr(ist, &thread_control[ist]);
                    }

                    // Thread tasks are set up so run them
                    T->run_thread_tasks(T->get_threads_per_node());

                }

                // Process any remaining states in serial fashion
                for(st1 = istop;st1 < Kptr[kpt]->nstates;st1++) {
                        MgEigState<std::complex<double>, std::complex<float> > ((Kpoint<std::complex<double>> *)Kptr[kpt], (State<std::complex<double> > *)&Kptr[kpt]->Kstates[st1], vtot_psi);
                }

            }  // end for vcycle

            max_res = Kptr[kpt]->Kstates[0].res;
            for(int istate = 0; istate < Kptr[kpt]->nstates; istate++)
                if( max_res < Kptr[kpt]->Kstates[istate].res) 
                    max_res = Kptr[kpt]->Kstates[istate].res;

            if (max_res <ct.gw_threshold) 
            {
                rmg_printf("\n BAND STRUCTURE: Converged with max_res %10.5e", max_res);
                CONVERGED = true;
            }

            /*wavefunctions have changed, projectors have to be recalculated */
            Betaxpsi (Kptr[kpt]);
            Kptr[kpt]->mix_betaxpsi(0);
            AppNls(Kptr[kpt], Kptr[kpt]->oldsint_local);

        } // end loop scf

        for(int istate = 0; istate < Kptr[kpt]->nstates; istate++)
        rmg_printf("\n BAND STRUCTURE: state %d res %10.5e ", istate, Kptr[kpt]->Kstates[istate].res);

    } // end loop over kpoints


    delete [] vtot;
    delete [] vtot_psi;
}

/******/




