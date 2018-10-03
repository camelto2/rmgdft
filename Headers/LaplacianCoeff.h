#ifndef RMG_LaplacianCoeff_H
#define RMG_LaplacianCoeff_H 1
#include "const.h"
// with input of base vectors a[3][3] and required order of Lapalacian operator, find needed neighbors and their coefficients. 

typedef struct {
    double coeff;
    std::vector<int> relative_index;
} CoeffList; 

class LaplacianCoeff {

private:

    double a[3][3];
    int Ngrid[3];
    int Lorder;
    int dim[3];
    int weight_power = 3;
    bool offdiag = true;
    int ibrav = ORTHORHOMBIC_PRIMITIVE;
    

public:
    

    LaplacianCoeff ();
    LaplacianCoeff (double a[3][3], int Ngrid[3], int Lorder, int dim[3]);
    void CalculateCoeff (double a[3][3], int Ngrid[3], int Lorder, int dim[3]);
    void CalculateCoeff ();
    ~LaplacianCoeff(void);

    std::vector<CoeffList> coeff_and_index;
    void SetLattice(double a[3][3])
    {
        for(int i = 0; i < 3; i++)
            for(int j = 0; j < 3; j++)
                this->a[i][j] = a[i][j];
    }
    void SetWeightPower(int wp){ this->weight_power = wp;}

    void SetOffdiag(bool flag){
         this->offdiag = flag;
        }
    void SetBrav(int ibrav){
         this->ibrav = ibrav;
        }
    void SetOrder(int Lorder){this->Lorder = Lorder;}
    void SetNgrid(int Ngrid[3]){
        for(int i = 0; i < 3; i++) this->Ngrid[i] = Ngrid[i];
}
    void SetDim(int dim[3]){
        for(int i = 0; i < 3; i++) this->dim[i] = dim[i];
}





};
#endif