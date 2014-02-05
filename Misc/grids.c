/*

  Data and functions related to grid layout and dimensions.

*/

#include "grid.h"
#include "main.h"
#include "common_prototypes.h"


/* Grid sizes on each PE */
static int PX0_GRID;
static int PY0_GRID;
static int PZ0_GRID;

/* Grid offsets on each PE */
static int PX_OFFSET;
static int PY_OFFSET;
static int PZ_OFFSET;

/* Basis size on each PE */
static int P0_BASIS;

/* Fine grid sizes on each PE */
static int FPX0_GRID;
static int FPY0_GRID;
static int FPZ0_GRID;

/* Fine Grid offsets on each PE */
static int FPX_OFFSET;
static int FPY_OFFSET;
static int FPZ_OFFSET;

/* Fine grid basis size on each PE */
static int FP0_BASIS;

/* Grid bravais lattice type */
static int ibrav;

static int neighbor_first=0;
static int grid_first=0;
static int neighbors[6];

/** Grid anisotropy defined as the ratio of hmaxgrid to hmingrid. A value larger than 1.05 can lead to convergence problems. */
static rmg_double_t anisotropy;


void set_grids(int ii, int jj, int kk)
{

    int rem;

    // Compute grid sizes for each node.
    PX0_GRID = NX_GRID / PE_X;
    rem = NX_GRID % PE_X;
    if(rem && (ii < rem)) PX0_GRID++;

    PY0_GRID = NY_GRID / PE_Y;
    rem = NY_GRID % PE_Y;
    if(rem && (jj < rem)) PY0_GRID++;

    PZ0_GRID = NZ_GRID / PE_Z;
    rem = NZ_GRID % PE_Z;
    if(rem && (kk < rem)) PZ0_GRID++;

    find_node_sizes(pct.gridpe, NX_GRID, NY_GRID, NZ_GRID, &PX0_GRID, &PY0_GRID, &PZ0_GRID);
    find_node_sizes(pct.gridpe, FNX_GRID, FNY_GRID, FNZ_GRID, &FPX0_GRID, &FPY0_GRID, &FPZ0_GRID);

    P0_BASIS = PX0_GRID * PY0_GRID * PZ0_GRID;
    FP0_BASIS = FPX0_GRID * FPY0_GRID * FPZ0_GRID;

    // Now compute the global grid offset of the first point of the coarse and fine node grids
    find_node_offsets(pct.gridpe, NX_GRID, NY_GRID, NZ_GRID,
                      &PX_OFFSET, &PY_OFFSET, &PZ_OFFSET);

    find_node_offsets(pct.gridpe, FNX_GRID, FNY_GRID, FNZ_GRID,
                      &FPX_OFFSET, &FPY_OFFSET, &FPZ_OFFSET);

    grid_first = 1;
}
int get_PX0_GRID(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return PX0_GRID;
}
int get_PY0_GRID(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return PY0_GRID;
}
int get_PZ0_GRID(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return PZ0_GRID;
}
int get_PX_OFFSET(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return PX_OFFSET;
}
int get_PY_OFFSET(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return PY_OFFSET;
}
int get_PZ_OFFSET(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return PZ_OFFSET;
}
int get_FPX_OFFSET(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return FPX_OFFSET;
}
int get_FPY_OFFSET(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return FPY_OFFSET;
}
int get_FPZ_OFFSET(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return FPZ_OFFSET;
}
int get_P0_BASIS(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return P0_BASIS;
}
int get_FP0_BASIS(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return FP0_BASIS;
}
int get_FPX0_GRID(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return FPX0_GRID;
}
int get_FPY0_GRID(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return FPY0_GRID;
}
int get_FPZ0_GRID(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return FPZ0_GRID;
}
int get_ibrav_type(void)
{
    if(!grid_first)
        error_handler("Grids not initialized. Please call set_grids first");
    return ibrav;
}
void set_ibrav_type(int value)
{
    ibrav = value;
}
void set_anisotropy(rmg_double_t a)
{
  anisotropy = a;
}
rmg_double_t get_anisotropy(void)
{
    return anisotropy;
}
void set_neighbors(int *list)
{
    int idx;
    
    for(idx = 0;idx < 6;idx++) 
        neighbors[idx] = list[idx];

    neighbor_first = 1;

}

// Returns a pointer to the neighbors structure which contains the rank
// of neighboring processors in three-dimensional space.
int *get_neighbors(void)
{
    if(!neighbor_first)
        error_handler("Neighbor list not initialized. Please call set_neighbors first");

    return neighbors;
}
