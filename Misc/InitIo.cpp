/*
 *
 * Copyright 2014 The RMG Project Developers. See the COPYRIGHT file 
 * at the top-level directory of this distribution or in the current
 * directory.
 * 
 * This file is part of RMG. 
 * RMG is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * any later version.
 *
 * RMG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
*/


/***** RMG: Common/init_IO.c *****
 * NAME
 *   Ab initio real space multigrid acceleration
 *   Quantum molecular dynamics package.
 * COPYRIGHT
 *   Copyright (C) 2009  Frisco Rose, Jerzy Bernholc
 * FUNCTION
 *   void init_IO( int argc, char **argv )
 *   Initializes settings and creates directory structures for ouput logging
 *   Make each run image manage its own directory of input/output
 * INPUTS
 *   argc and argv from main
 * OUTPUT
 *   none
 * PARENTS
 *   main.c
 * CHILDREN
 *   init_pe.c read_pseudo.c
 * SOURCE
 */

#include "portability.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <unordered_map>
#if (defined(_WIN32) || defined(_WIN64))
    #include <io.h>
#else
    #include <unistd.h>
#endif


#include "grid.h"
#include "const.h"
#include "params.h"
#include "rmg_alloc.h"
#include "rmgtypedefs.h"
#include "typedefs.h"
#include "common_prototypes.h"
#include "common_prototypes1.h"
#include "transition.h"
#include "macros.h"
#include "GlobalSums.h"
#include "RmgException.h"
#include "InputKey.h"
#include "hybrid.h"

#if GPU_ENABLED
    #if MAGMA_LIBS
        #include <magma.h>
        #include <magmablas.h>
    #endif

    #include <cuda.h>
    #include <cuda_runtime_api.h>
    #include <cublasXt.h>
    #include <cublas_v2.h>

#endif

extern "C" void dgemm_(void);
extern "C" void zgemm_(void);

void InitIo (int argc, char **argv, std::unordered_map<std::string, InputKey *>& ControlMap)
{

    int npes, worldpe, status, provided=0;
    time_t timer;

    /* Set start of program time */
    timer = time (NULL);
    MPI_Init_thread(&argc, &argv, ct.mpi_threadlevel, &provided);

    /* get this cores mpi rank */
    MPI_Comm_rank (MPI_COMM_WORLD, &worldpe);
    pct.worldrank = worldpe;

    /* get total mpi core count */
    MPI_Comm_size (MPI_COMM_WORLD, &npes);
    pct.total_npes = npes;

    if(argc == 2)
    {
        std::unordered_map<std::string, InputKey *> SetupMap;
        ReadInit(argv[1], ct, pct, SetupMap);

    }
    else {

        std::unordered_map<std::string, InputKey *> SetupMap;
        ReadInit("ctrl_init.dat", ct, pct, SetupMap);

    }

    init_pestr ();

    snprintf (ct.cfile, MAX_PATH, "%s%s", pct.image_path[pct.thisimg], pct.image_input[pct.thisimg]);
    snprintf (ct.shortname, MAX_PATH, "%s%s", pct.image_path[pct.thisimg], pct.image_input[pct.thisimg]);

    ReadCommon(argc, argv, ct.cfile, ct, pct, ControlMap);
    if(Verify("start_mode", "Restart From File", ControlMap)) {
        std::string dynfile(ct.infile);
        dynfile = dynfile + ".restart";
        ReadDynamics((char *)dynfile.c_str(), ct, ControlMap);

        // Always use absolute coords in restart file
        InputKey *Ik = ControlMap["atomic_coordinate_type"];
        static std::string AbsoluteCoords("Absolute");
        Ik->Readstr = AbsoluteCoords;
    }
    else {
        ReadDynamics(ct.cfile, ct, ControlMap);
    }

    if((ct.kpoint_mesh[0] < 1) | (ct.kpoint_mesh[1] < 1) | (ct.kpoint_mesh[2] < 1) ) 
    {
        ReadKpoints(ct.cfile, ct, ControlMap);
    }
    else
    {
        init_kpoints(ct.kpoint_mesh, ct.kpoint_is_shift);
    }

    Rmg_G->set_rank(pct.gridpe, pct.grid_comm);

    InitHybridModel(ct.THREADS_PER_NODE, NPES, pct.gridpe, pct.grid_comm);

    /* if logname exists, increment until unique filename found */
    if (pct.imgpe == 0)
    {
        int name_incr;
        name_incr = FilenameIncrement(ct.shortname);
        snprintf (ct.basename, MAX_PATH, "%s.%02d", ct.shortname, name_incr);
        snprintf (ct.logname, MAX_PATH, "%s.%02d.log", ct.shortname, name_incr);

        /* open and save logfile handle, printf is stdout before here */
        ct.logfile = fopen(ct.logname, "w");
    }
    else {
#if (defined(_WIN32) || defined(_WIN64))
        ct.logfile = fopen("NUL:", "w");
#else
        ct.logfile = fopen("/dev/null", "w");
#endif
    }

    MPI_Comm_size (pct.img_comm, &status);
    printf ("\nRMG run started at GMT %s", asctime (gmtime (&timer)));
    printf ("\nRMG running with %d images and %d images per node.\n", pct.images, ct.images_per_node);
    printf ("\nRMG running in message passing mode with %d procs for this image.", status);

    /* Read in our pseudopotential information */
    ReadPseudo(ct.num_species, ct, ControlMap);


#if GPU_ENABLED
    cudaDeviceReset();
    cudaSetDeviceFlags(cudaDeviceScheduleSpin);
    if( CUDA_SUCCESS != cuInit( 0 ) ) {
        fprintf(stderr, "CUDA: Not initialized\n" ); exit(-1);
    }
    if( CUDA_SUCCESS != cuDeviceGet( &ct.cu_dev, 0 ) ) {
        fprintf(stderr, "CUDA: Cannot get the device\n"); exit(-1);
    }
    cudaSetDevice(ct.cu_dev);
    if( CUBLAS_STATUS_SUCCESS != cublasCreate(&ct.cublas_handle) ) {
        fprintf(stderr, "CUBLAS: Handle not created\n"); exit(-1);
    }

    if( CUBLAS_STATUS_SUCCESS != cublasXtCreate(&ct.cublasXt_handle) ) {
        fprintf(stderr, "CUBLASXT: Handle not created\n"); exit(-1);
    }
    int devices[1] = { 0 };
    if(cublasXtDeviceSelect(ct.cublasXt_handle, 1, devices) != CUBLAS_STATUS_SUCCESS) {
        fprintf(stderr, "XT set devices fail\n"); exit(-1);
    } //

    cublasXtSetBlockDim(ct.cublasXt_handle, 2048);
    void *fptr;
//    fptr = (void *)&dgemm_;
//    cublasXtSetCpuRoutine(ct.cublasXt_handle, CUBLASXT_GEMM, CUBLASXT_DOUBLE, fptr);
//    fptr = (void *)&zgemm_;
//    cublasXtSetCpuRoutine(ct.cublasXt_handle, CUBLASXT_GEMM, CUBLASXT_COMPLEX, fptr);
//    cublasXtSetCpuRatio(ct.cublasXt_handle, CUBLASXT_GEMM, CUBLASXT_DOUBLE, 0.5);

#if MAGMA_LIBS
    magma_init();
    //magmablasSetKernelStream(ct.cuda_stream);
#endif

#endif

    // This is placed down here since the IO is not setup yet when provided is obtained above.
    if(provided < ct.mpi_threadlevel) {

        printf("Thread support requested = %d but only %d provided. Terminating.\n", ct.mpi_threadlevel, provided);
        MPI_Finalize();
        exit(0);

    }
    printf("Running with thread level = %d\n", provided);
    fflush(NULL);

    // Allocate storage for trade_images and global sums routines
    Rmg_T = new TradeImages(Rmg_G);
    Rmg_T->set_MPI_comm(pct.grid_comm);

    GlobalSumsInit();

    // Write a copy of the options file
    if(pct.imgpe == 0) {

        // Write out options file
        std::string OptionsFile(ct.basename);
        OptionsFile = OptionsFile + ".options";

        FILE *fhand = fopen(OptionsFile.c_str(), "w");
        if (!fhand)
            throw RmgFatalException() <<  "Unable to write file in " << __FILE__ << " at line " << __LINE__ << "\n";

        for(auto it = ControlMap.begin();it != ControlMap.end(); ++it) {

            std::string KeyName = it->first;
            InputKey *ik = it->second;
            std::string KeyVal = ik->Print();
            fprintf(fhand, "%s = \"%s\"\n", KeyName.c_str(), KeyVal.c_str());fflush(NULL);
        }

        fclose(fhand);
    }

}

// Required here for transitional routines
extern std::unordered_map<std::string, InputKey *> ControlMap;

extern "C" void init_IO(int argc, char **argv)
{
    InitIo(argc, argv, ControlMap);
}

