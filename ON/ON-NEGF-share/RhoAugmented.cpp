/************************** SVN Revision Information **************************
 **    $Id$    **
******************************************************************************/
 
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>


#include "params.h"

#include "rmgtypedefs.h"
#include "typedefs.h"
#include "RmgTimer.h"


#include "prototypes_on.h"

void RhoAugmented(double * rho, double * global_mat_X)

{

    int idx;
    int *ivec, size, idx1, idx2;
    int nh, icount, ncount, i, j, ion;
    double *qnmI, *qtpr;
    double *product, *ptr_product;
    ION *iptr;
    SPECIES *sp;

    size = ct.num_ions * ct.max_nl * ct.max_nl;
    product = new double[size];
    for (idx = 0; idx < size; idx++)
        product[idx] = 0.0;


    RmgTimer *RT5 = new RmgTimer("3-get_new_rho: augmented_qnm");
    RhoQnmMat(product, global_mat_X);
    delete(RT5);

    global_sums(product, &size, pct.grid_comm);

    RmgTimer *RT6 = new RmgTimer("3-get_new_rho: augmented_Q(r)");
    for (ion = 0; ion < ct.num_ions; ion++)
    {
        iptr = &ct.ions[ion];
        sp = &ct.sp[iptr->species];
        nh = sp->num_projectors;
        ptr_product = product + ion * ct.max_nl * ct.max_nl;
        ivec = pct.Qindex[ion];
        ncount = pct.Qidxptrlen[ion];
        qnmI = pct.augfunc[ion];

        if (pct.Qidxptrlen[ion])
        {

            idx = 0;
            for (i = 0; i < nh; i++)
            {
                for (j = i; j < nh; j++)
                {
                    idx1 = i * ct.max_nl + j;
                    idx2 = j * ct.max_nl + i;
                    qtpr = qnmI + idx * ncount;
                    for (icount = 0; icount < ncount; icount++)
                    {
                        if (i != j)
                            rho[ivec[icount]] +=
                                qtpr[icount] * (ptr_product[idx1] + ptr_product[idx2]);
                        else
                            rho[ivec[icount]] += qtpr[icount] * ptr_product[idx1];
                    }           /*end for icount */
                    idx++;
                }               /*end for j */
            }                   /*end for i */

        }                       /*end if */

    }                           /*end for ion */

    delete(RT6);
    /* release our memory */
    delete [] product;

}

