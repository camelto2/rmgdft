
#include <float.h>
#include <math.h>
#include "main.h"
#include "Atomic.h"
#include "Pw.h"
#include "Lattice.h"
#include "transition.h"

void compute_der(int num_orb, int dimx, int dimy, int dimz, int pbasis, int sbasis, 
        int num_coeff, int order, double *orbitals, double * orbitals_b, double *psi_psin);
void compute_coeff_grad(int num_orb, std::vector<double> ke_fft, std::vector<double> ke_fd, 
        double *psi_psin, std::vector<double>& coeff_grad);
double ComputeKineticEnergy(double *x, double *lapx, int pbasis);


void OptimizeFdCoeff()
{
    FiniteDiff FD(&Rmg_L);
    std::complex<double> I_t(0.0, 1.0);

    int nlxdim = get_NX_GRID();
    int nlydim = get_NY_GRID();
    int nlzdim = get_NZ_GRID();
    int pbasis = Rmg_G->get_P0_BASIS(1);
    int dimx  = Rmg_G->get_PX0_GRID(1);
    int dimy  = Rmg_G->get_PY0_GRID(1);
    int dimz  = Rmg_G->get_PZ0_GRID(1);
    int order = LC->Lorder;
    int num_coeff;
    int sbasis = (dimx + order) * (dimy + order) * (dimz + order);

    std::complex<double> *fftw_phase = new std::complex<double>[pbasis];

    //  determine total number of orbital to be included, sum of atomic orbitals for each speices

    int num_orb = 0;
    for (auto& sp : Species)
    {
        num_orb += sp.num_orbitals;
        sp.num_atoms = 0;
    }
    for(auto& Atom : Atoms)
    {
        Atom.Type->num_atoms +=1;
    }

    // determine and initialize coeffients

    std::vector<double> coeff;
    std::vector<double> coeff_grad;
    for (int ax = 0; ax < 13; ax++)
    {
        if(!LC->include_axis[ax]) continue;
        for(int i = 0; i < LC->Lorder/2; i++)
        {
            coeff.push_back(LC->axis_lc[ax][i]);
        } 
    }
    coeff_grad.resize(coeff.size());
    num_coeff = coeff.size();

    double *orbitals = new double[num_orb * pbasis];
    double *orbitals_b = new double[num_orb * sbasis];
    double *psi_psin = new double[num_orb * coeff.size()];

    std::vector<double> ke_fft;
    std::vector<double> ke_fd;
    std::vector<double> occ_weight;
    ke_fft.resize(num_orb);
    ke_fd.resize(num_orb);

    for (auto& sp : Species)
    {
        // Set up an occupation weight array
        for (int ip = 0; ip < sp.num_atomic_waves; ip++)
        {
            // This is here since we have forward beta only for occupied orbitals.
            // If that changes then this must change.
            if(sp.atomic_wave_oc[ip] > 0.0)
            {
                for(int m = 0; m < 2*sp.atomic_wave_l[ip]+1; m++)
                {
                    occ_weight.push_back(sp.atomic_wave_oc[ip] / (2*sp.atomic_wave_l[ip]+1) * sp.num_atoms);
                }
            }
        }
    }

    if(occ_weight.size() - num_orb != 0)
    {
        printf("\n occ_weigh size %d != num_orb %d", (int)occ_weight.size(), num_orb);
    }


    std::complex<double> *beptr = (std::complex<double> *)fftw_malloc(sizeof(std::complex<double>) * pbasis);
    std::complex<double> *gbptr = (std::complex<double> *)fftw_malloc(sizeof(std::complex<double>) * pbasis);
    double *work = new double[pbasis];
    double vect[3], nlcrds[3], kvec[3];

    /* Find nlcdrs, vector that gives shift of ion from center of its ionic box */
    /* for delocalized case it's just half the cell dimensions */
    vect[0] = 0.5;
    vect[1] = 0.5;
    vect[2] = 0.5;
    kvec[0] = 0.0;
    kvec[1] = 0.0;
    kvec[2] = 0.0;

    // Loop over species
    int orb_idx = 0;
    int images = LC->Lorder/2;
    for (auto& sp : Species)
    {

        /*The vector we are looking for should be */
        to_cartesian (vect, nlcrds);

        /*Calculate the phase factor */
        FindPhaseKpoint (kvec, nlxdim, nlydim, nlzdim, nlcrds, fftw_phase, false);

        /*Temporary pointer to the already calculated forward transform. */
        /* Need to fix up for kpoint parrallelization issues.  */
        std::complex<double> *fptr = (std::complex<double> *)sp.forward_orbital;

        /* Loop over atomic orbitals */
        for (int ip = 0; ip < sp.num_orbitals; ip++)
        {
            /*Apply the phase factor */
            for (int idx = 0; idx < pbasis; idx++) gbptr[idx] = fptr[idx] * std::conj(fftw_phase[idx]);

            /*Do the backwards transform */
            coarse_pwaves->FftInverse(gbptr, beptr);

            for (int idx = 0; idx < pbasis; idx++) orbitals[orb_idx*pbasis + idx] = std::real(beptr[idx]);

            Rmg_T->trade_imagesx (&orbitals[orb_idx*pbasis], &orbitals_b[orb_idx*sbasis], dimx, dimy, dimz, images, FULL_TRADE);

            /*Advance the fortward transform pointers */
            fptr += pbasis;

            // Get the FFT laplacian and compute the kinetic energy as our gold standard
            FftLaplacianCoarse(&orbitals[orb_idx*pbasis], work);
            ke_fft[orb_idx] = ComputeKineticEnergy(&orbitals[orb_idx*pbasis], work, pbasis);
            orb_idx++;
        }
    }


    fftw_free (gbptr);
    fftw_free (beptr);
    delete [] fftw_phase;


    // calculte <psi(ijk)| psi(ijk + neighbor)> which will never change with different coeffients.
    compute_der(num_orb, dimx, dimy, dimz, pbasis, sbasis, num_coeff, order, orbitals, orbitals_b, psi_psin);


    double ke_diff2;
    int iter_max = 20;
    lbfgs_init(num_coeff);
    for(int iter = 0; iter < iter_max; iter++)
    {
        int icoeff = 0;
        for (int ax = 0; ax < 13; ax++)
        {
            if(!LC->include_axis[ax]) continue;
            for(int i = 0; i < LC->Lorder/2; i++)
            {
                LC->axis_lc[ax][i] = coeff[icoeff];
                icoeff++;
            } 
        }
        ke_diff2 = 0.0;
        for(int iorb = 0; iorb < num_orb; iorb++)
        {
            ApplyAOperator (&orbitals[iorb*pbasis], work, kvec);
            ke_fd[iorb] = ComputeKineticEnergy(&orbitals[iorb*pbasis], work, pbasis);
            ke_diff2 += (ke_fd[iorb] - ke_fft[iorb]) *(ke_fd[iorb] - ke_fft[iorb]);
        }
        compute_coeff_grad(num_orb, ke_fft, ke_fd, psi_psin, coeff_grad);

        if(pct.gridpe == 0) 
        {
            printf("\n iter %d  ke_diff2 = %e", iter, ke_diff2);
            for(int ic = 0; ic < num_coeff; ic++)
            {
              //  printf("\n coeff  %e   grad  %e ", coeff[ic], coeff_grad[ic]);
            }
        }
        

        lbfgs(coeff.data(), coeff_grad.data(), num_coeff);
    }

    delete [] orbitals;
    delete [] orbitals_b;
    delete [] psi_psin;
    delete [] work;
}

void compute_coeff_grad(int num_orb, std::vector<double> ke_fft, std::vector<double> ke_fd, double *psi_psin, std::vector<double> &coeff_grad)
{
    std::fill(coeff_grad.begin(), coeff_grad.end(), 0.0);
    int num_coeff = coeff_grad.size();
    for(int iorb = 0; iorb < num_orb; iorb++)
    {
        for(int i = 0; i < num_coeff; i++)
        {
            coeff_grad[i] += 2.0 * (ke_fd[iorb] - ke_fft[iorb]) * psi_psin[iorb * num_coeff + i];
        }
    }

}
void compute_der(int num_orb, int dimx, int dimy, int dimz, int pbasis, int sbasis, int num_coeff, int order, double *orbitals, double * orbitals_b, double *psi_psin)
{
    for(int i = 0; i < num_orb * num_coeff; i++) psi_psin[i] = 0.0;
    if(order != 8) 
    {
        printf("\n only 8th order is donw \n");
        fflush(NULL);
        exit(0);
    }

    int ixs = (dimy + 8) * (dimz + 8);
    int iys = (dimz + 8);
    int izs = 1;
    for (int iorb = 0; iorb < num_orb; iorb++)
    {
        double *b = &orbitals[iorb * pbasis]; 
        double *a = &orbitals_b[iorb * sbasis]; 
        int icoeff = 0;

        // take care of 3 axis along lattice vectors.
        for (int ix = 4; ix < dimx + 4; ix++)
        {
            for (int iy = 4; iy < dimy + 4; iy++)
            {
                double *A = &a[iy*iys + ix*ixs];
                double *B = &b[(iy - 4)*dimz + (ix - 4)*dimy*dimz - 4];
                // z-direction is orthogonal to xy-plane and only requires increments/decrements along z
                // 0=x,1=y,2=z,3=xy,4=xz,5=yz,6=nxy,7=nxz,8=nyz
                for (int iz = 4; iz < dimz + 4; iz++)
                {
                    psi_psin[iorb * num_coeff + 0]  +=  B[iz] * (A[iz + 1 * ixs] +  A[iz - 1 * ixs]);
                    psi_psin[iorb * num_coeff + 1]  +=  B[iz] * (A[iz + 2 * ixs] +  A[iz - 2 * ixs]);
                    psi_psin[iorb * num_coeff + 2]  +=  B[iz] * (A[iz + 3 * ixs] +  A[iz - 3 * ixs]);
                    psi_psin[iorb * num_coeff + 3]  +=  B[iz] * (A[iz + 4 * ixs] +  A[iz - 4 * ixs]);

                    psi_psin[iorb * num_coeff + 4]  +=  B[iz] * (A[iz + 1 * iys] +  A[iz - 1 * iys]);
                    psi_psin[iorb * num_coeff + 5]  +=  B[iz] * (A[iz + 2 * iys] +  A[iz - 2 * iys]);
                    psi_psin[iorb * num_coeff + 6]  +=  B[iz] * (A[iz + 3 * iys] +  A[iz - 3 * iys]);
                    psi_psin[iorb * num_coeff + 7]  +=  B[iz] * (A[iz + 4 * iys] +  A[iz - 4 * iys]);

                    psi_psin[iorb * num_coeff + 8]  +=  B[iz] * (A[iz + 1 * izs] +  A[iz - 1 * izs]);
                    psi_psin[iorb * num_coeff + 9]  +=  B[iz] * (A[iz + 2 * izs] +  A[iz - 2 * izs]);
                    psi_psin[iorb * num_coeff + 10] +=  B[iz] * (A[iz + 3 * izs] +  A[iz - 3 * izs]);
                    psi_psin[iorb * num_coeff + 11] +=  B[iz] * (A[iz + 4 * izs] +  A[iz - 4 * izs]);
                }

            }
        }


        // Add additional axes as required
        icoeff = 12;
        if(LC->include_axis[3])
        {
            for (int ix = 4; ix < dimx + 4; ix++)
            {
                for (int iy = 4; iy < dimy + 4; iy++)
                {
                    double *A = &a[iy*iys + ix*ixs];
                    double *B = &b[(iy - 4)*dimz + (ix - 4)*dimy*dimz - 4];
                    for (int iz = 4; iz < dimz + 4; iz++)
                    {
                        psi_psin[iorb * num_coeff + icoeff + 1]  +=  B[iz] * (A[iz + 1 * ixs + 1 * iys] +  A[iz - 1 * ixs - 1 * iys]);
                        psi_psin[iorb * num_coeff + icoeff + 2]  +=  B[iz] * (A[iz + 2 * ixs + 2 * iys] +  A[iz - 2 * ixs - 2 * iys]);
                        psi_psin[iorb * num_coeff + icoeff + 3]  +=  B[iz] * (A[iz + 3 * ixs + 3 * iys] +  A[iz - 3 * ixs - 3 * iys]);
                        psi_psin[iorb * num_coeff + icoeff + 4]  +=  B[iz] * (A[iz + 4 * ixs + 4 * iys] +  A[iz - 4 * ixs - 4 * iys]);
                    }                   /* end for */
                }
            }
            icoeff += 4;
        }

        if(LC->include_axis[4])
        {
            for (int ix = 4; ix < dimx + 4; ix++)
            {
                for (int iy = 4; iy < dimy + 4; iy++)
                {
                    double *A = &a[iy*iys + ix*ixs];
                    double *B = &b[(iy - 4)*dimz + (ix - 4)*dimy*dimz - 4];
                    for (int iz = 4; iz < dimz + 4; iz++)
                    {
                        psi_psin[iorb * num_coeff + icoeff + 1]  +=  B[iz] * (A[iz + 1 * ixs + 1 * izs] +  A[iz - 1 * ixs - 1 * izs]);
                        psi_psin[iorb * num_coeff + icoeff + 2]  +=  B[iz] * (A[iz + 2 * ixs + 2 * izs] +  A[iz - 2 * ixs - 2 * izs]);
                        psi_psin[iorb * num_coeff + icoeff + 3]  +=  B[iz] * (A[iz + 3 * ixs + 3 * izs] +  A[iz - 3 * ixs - 3 * izs]);
                        psi_psin[iorb * num_coeff + icoeff + 4]  +=  B[iz] * (A[iz + 4 * ixs + 4 * izs] +  A[iz - 4 * ixs - 4 * izs]);
                    }                   /* end for */
                }
            }
            icoeff += 4;
        }

        if(LC->include_axis[5])
        {
            for (int ix = 4; ix < dimx + 4; ix++)
            {
                for (int iy = 4; iy < dimy + 4; iy++)
                {
                    double *A = &a[iy*iys + ix*ixs];
                    double *B = &b[(iy - 4)*dimz + (ix - 4)*dimy*dimz - 4];
                    for (int iz = 4; iz < dimz + 4; iz++)
                    {
                        psi_psin[iorb * num_coeff + icoeff + 1]  +=  B[iz] * (A[iz + 1 * iys + 1 * izs] +  A[iz - 1 * iys - 1 * izs]);
                        psi_psin[iorb * num_coeff + icoeff + 2]  +=  B[iz] * (A[iz + 2 * iys + 2 * izs] +  A[iz - 2 * iys - 2 * izs]);
                        psi_psin[iorb * num_coeff + icoeff + 3]  +=  B[iz] * (A[iz + 3 * iys + 3 * izs] +  A[iz - 3 * iys - 3 * izs]);
                        psi_psin[iorb * num_coeff + icoeff + 4]  +=  B[iz] * (A[iz + 4 * iys + 4 * izs] +  A[iz - 4 * iys - 4 * izs]);
                    }                   /* end for */
                }
            }
            icoeff += 4;
        }

        if(LC->include_axis[6])
        {
            for (int ix = 4; ix < dimx + 4; ix++)
            {
                for (int iy = 4; iy < dimy + 4; iy++)
                {
                    double *A = &a[iy*iys + ix*ixs];
                    double *B = &b[(iy - 4)*dimz + (ix - 4)*dimy*dimz - 4];
                    for (int iz = 4; iz < dimz + 4; iz++)
                    {
                        psi_psin[iorb * num_coeff + icoeff + 1]  +=  B[iz] * (A[iz + 1 * ixs + 1 * iys] +  A[iz - 1 * ixs - 1 * iys]);
                        psi_psin[iorb * num_coeff + icoeff + 2]  +=  B[iz] * (A[iz + 2 * ixs + 2 * iys] +  A[iz - 2 * ixs - 2 * iys]);
                        psi_psin[iorb * num_coeff + icoeff + 3]  +=  B[iz] * (A[iz + 3 * ixs + 3 * iys] +  A[iz - 3 * ixs - 3 * iys]);
                        psi_psin[iorb * num_coeff + icoeff + 4]  +=  B[iz] * (A[iz + 4 * ixs + 4 * iys] +  A[iz - 4 * ixs - 4 * iys]);
                    }                   /* end for */
                }
            }
            icoeff += 4;
        }

        if(LC->include_axis[7])
        {
            for (int ix = 4; ix < dimx + 4; ix++)
            {
                for (int iy = 4; iy < dimy + 4; iy++)
                {
                    double *A = &a[iy*iys + ix*ixs];
                    double *B = &b[(iy - 4)*dimz + (ix - 4)*dimy*dimz - 4];
                    for (int iz = 4; iz < dimz + 4; iz++)
                    {
                        psi_psin[iorb * num_coeff + icoeff + 1]  +=  B[iz] * (A[iz - 1 * ixs + 1 * izs] +  A[iz + 1 * ixs - 1 * izs]);
                        psi_psin[iorb * num_coeff + icoeff + 2]  +=  B[iz] * (A[iz - 2 * ixs + 2 * izs] +  A[iz + 2 * ixs - 2 * izs]);
                        psi_psin[iorb * num_coeff + icoeff + 3]  +=  B[iz] * (A[iz - 3 * ixs + 3 * izs] +  A[iz + 3 * ixs - 3 * izs]);
                        psi_psin[iorb * num_coeff + icoeff + 4]  +=  B[iz] * (A[iz - 4 * ixs + 4 * izs] +  A[iz + 4 * ixs - 4 * izs]);
                    }                   /* end for */
                }
            }
            icoeff += 4;
        }

        if(LC->include_axis[8])
        {
            for (int ix = 4; ix < dimx + 4; ix++)
            {
                for (int iy = 4; iy < dimy + 4; iy++)
                {
                    double *A = &a[iy*iys + ix*ixs];
                    double *B = &b[(iy - 4)*dimz + (ix - 4)*dimy*dimz - 4];
                    for (int iz = 4; iz < dimz + 4; iz++)
                    {
                        psi_psin[iorb * num_coeff + icoeff + 1]  +=  B[iz] * (A[iz - 1 * iys + 1 * izs] +  A[iz + 1 * iys - 1 * izs]);
                        psi_psin[iorb * num_coeff + icoeff + 2]  +=  B[iz] * (A[iz - 2 * iys + 2 * izs] +  A[iz + 2 * iys - 2 * izs]);
                        psi_psin[iorb * num_coeff + icoeff + 3]  +=  B[iz] * (A[iz - 3 * iys + 3 * izs] +  A[iz + 3 * iys - 3 * izs]);
                        psi_psin[iorb * num_coeff + icoeff + 4]  +=  B[iz] * (A[iz - 4 * iys + 4 * izs] +  A[iz + 4 * iys - 4 * izs]);
                    }                   /* end for */
                }
            }
            icoeff += 4;
        }

        if(LC->include_axis[9])
        {
            for (int ix = 4; ix < dimx + 4; ix++)
            {
                for (int iy = 4; iy < dimy + 4; iy++)
                {
                    double *A = &a[iy*iys + ix*ixs];
                    double *B = &b[(iy - 4)*dimz + (ix - 4)*dimy*dimz - 4];
                    for (int iz = 4; iz < dimz + 4; iz++)
                    {
                        psi_psin[iorb * num_coeff + icoeff + 1]  +=  B[iz] * (A[iz - 1 * ixs + 1 * iys] +  A[iz + 1 * ixs - 1 * iys]);
                        psi_psin[iorb * num_coeff + icoeff + 2]  +=  B[iz] * (A[iz - 2 * ixs + 2 * iys] +  A[iz + 2 * ixs - 2 * iys]);
                        psi_psin[iorb * num_coeff + icoeff + 3]  +=  B[iz] * (A[iz - 3 * ixs + 3 * iys] +  A[iz + 3 * ixs - 3 * iys]);
                        psi_psin[iorb * num_coeff + icoeff + 4]  +=  B[iz] * (A[iz - 4 * ixs + 4 * iys] +  A[iz + 4 * ixs - 4 * iys]);
                    }                   /* end for */
                }
            }
            icoeff += 4;
        }

        if(LC->include_axis[10])
        {
            for (int ix = 4; ix < dimx + 4; ix++)
            {
                for (int iy = 4; iy < dimy + 4; iy++)
                {
                    double *A = &a[iy*iys + ix*ixs];
                    double *B = &b[(iy - 4)*dimz + (ix - 4)*dimy*dimz - 4];
                    for (int iz = 4; iz < dimz + 4; iz++)
                    {
                        psi_psin[iorb * num_coeff + icoeff + 1]  +=  B[iz] * (A[iz - 1 * ixs - 1 * iys + 1 *izs] +  A[iz + 1 * ixs + 1 * iys - 1 *izs]);
                        psi_psin[iorb * num_coeff + icoeff + 2]  +=  B[iz] * (A[iz - 2 * ixs - 2 * iys + 2 *izs] +  A[iz + 2 * ixs + 2 * iys - 2 *izs]);
                        psi_psin[iorb * num_coeff + icoeff + 3]  +=  B[iz] * (A[iz - 3 * ixs - 3 * iys + 3 *izs] +  A[iz + 3 * ixs + 3 * iys - 3 *izs]);
                        psi_psin[iorb * num_coeff + icoeff + 4]  +=  B[iz] * (A[iz - 4 * ixs - 4 * iys + 4 *izs] +  A[iz + 4 * ixs + 4 * iys - 4 *izs]);
                    }                   /* end for */
                }
            }
            icoeff += 4;
        }

        if(LC->include_axis[11])
        {
            for (int ix = 4; ix < dimx + 4; ix++)
            {
                for (int iy = 4; iy < dimy + 4; iy++)
                {
                    double *A = &a[iy*iys + ix*ixs];
                    double *B = &b[(iy - 4)*dimz + (ix - 4)*dimy*dimz - 4];
                    for (int iz = 4; iz < dimz + 4; iz++)
                    {
                        psi_psin[iorb * num_coeff + icoeff + 1]  +=  B[iz] * (A[iz + 1 * ixs - 1 * iys + 1 *izs] +  A[iz - 1 * ixs + 1 * iys - 1 *izs]);
                        psi_psin[iorb * num_coeff + icoeff + 2]  +=  B[iz] * (A[iz + 2 * ixs - 2 * iys + 2 *izs] +  A[iz - 2 * ixs + 2 * iys - 2 *izs]);
                        psi_psin[iorb * num_coeff + icoeff + 3]  +=  B[iz] * (A[iz + 3 * ixs - 3 * iys + 3 *izs] +  A[iz - 3 * ixs + 3 * iys - 3 *izs]);
                        psi_psin[iorb * num_coeff + icoeff + 4]  +=  B[iz] * (A[iz + 4 * ixs - 4 * iys + 4 *izs] +  A[iz - 4 * ixs + 4 * iys - 4 *izs]);
                    }                   /* end for */
                }
            }
            icoeff += 4;
        }

        if(LC->include_axis[12])
        {
            for (int ix = 4; ix < dimx + 4; ix++)
            {
                for (int iy = 4; iy < dimy + 4; iy++)
                {
                    double *A = &a[iy*iys + ix*ixs];
                    double *B = &b[(iy - 4)*dimz + (ix - 4)*dimy*dimz - 4];
                    for (int iz = 4; iz < dimz + 4; iz++)
                    {
                        psi_psin[iorb * num_coeff + icoeff + 1]  +=  B[iz] * (A[iz + 1 * ixs - 1 * iys - 1 *izs] +  A[iz - 1 * ixs + 1 * iys + 1 *izs]);
                        psi_psin[iorb * num_coeff + icoeff + 2]  +=  B[iz] * (A[iz + 2 * ixs - 2 * iys - 2 *izs] +  A[iz - 2 * ixs + 2 * iys + 2 *izs]);
                        psi_psin[iorb * num_coeff + icoeff + 3]  +=  B[iz] * (A[iz + 3 * ixs - 3 * iys - 3 *izs] +  A[iz - 3 * ixs + 3 * iys + 3 *izs]);
                        psi_psin[iorb * num_coeff + icoeff + 4]  +=  B[iz] * (A[iz + 4 * ixs - 4 * iys - 4 *izs] +  A[iz - 4 * ixs + 4 * iys + 4 *izs]);
                    }                   /* end for */
                }
            }
            icoeff += 4;
        }

    }

    int idim = num_orb * num_coeff;
    MPI_Allreduce(MPI_IN_PLACE, psi_psin, idim, MPI_DOUBLE, MPI_SUM, pct.grid_comm);

    for(int idx = 0; idx < num_orb * num_coeff; idx++) psi_psin[idx] *= -0.5 * get_vel();
}