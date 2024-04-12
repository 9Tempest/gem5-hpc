#include "dataset1.h"
#include<random>
#include<cmath>
#include<iomanip>
#include<stdio.h>
#include<limits.h>
#include<stdlib.h>
#include<iostream>
#include<chrono>
#include<omp.h>

#define GEM5
#ifdef GEM5
#include <gem5/m5ops.h>
#endif


int spmv(int r, const double* val, const int* idx, const double* x,
          const int* ptr, double* y)
{
 int sum;
  for (int i = 0; i < r; i++)
  {
    int k;
    double yi0 = 0, yi1 = 0, yi2 = 0, yi3 = 0;
    for (k = ptr[i]; k < ptr[i+1]-3; k+=4)
    {
      yi0 += val[k+0]*x[idx[k+0]];
      yi1 += val[k+1]*x[idx[k+1]];
      yi2 += val[k+2]*x[idx[k+2]];
      yi3 += val[k+3]*x[idx[k+3]];
    }
    for ( ; k < ptr[i+1]; k++)
    {
      yi0 += val[k]*x[idx[k]];
    }
    y[i] = (yi0+yi1)+(yi2+yi3);
    sum += y[i];
  }
  return sum;
}

struct VeryLargeStruct{
    double a[12];
};




void out_of_range() {
    std :: cout << "error: the size of the matrix should be 1 to " << UINT_MAX
        << std :: endl;
    std :: cout << std :: endl;
}

int main(int argc, char *argv[]) {

    // We are tracking the wall clock time required to execute the matrix
    // multiply program.
    //
    auto prog_start = std :: chrono :: high_resolution_clock :: now();
    double y[R];

    // Setting the size of the matrix (N x N). M = 10
    //
    const unsigned int N = 1000;

    if(!(N > 1 && N < UINT_MAX)) {
        out_of_range();
        return -1;
    }
    // Initializing the matrix here. We are using a random distribution to
    // initialize the matrix.
    //
    std :: random_device rd;
    std :: mt19937 gen(rd());
    std :: uniform_real_distribution<> dis(0, 1);

    double *data_A = new double[N * N];
    double *data_B = new double[N * N];
    double *data_C = new double[N * N];

    double **A = new double*[N];
    double **B = new double*[N];
    double **C_M = new double*[N];

    for(int i = 0 ; i < N ; i++) {
        A[i] = &data_A[N * i];
        B[i] = &data_B[N * i];
        C_M[i] = &data_C[N * i];
        for(int j = 0 ; j < N ; j++) {
            A[i][j] = dis(gen);
            B[i][j] = dis(gen);
            C_M[i][j] = 0;
        }
    }

    // Naive matrix multiplication code. It performs N^3 computations. We also
    // keep a track of time for this part of the code.
    //
    auto mm_start = std :: chrono :: high_resolution_clock :: now();

    // annotating the ROI
    //
// #ifdef GEM5
//     m5_work_end(0, 0);
// #endif
VeryLargeStruct vls[N];
std::cout << "Printed before checkpoint!!!" << std::endl;
#ifdef GEM5
    m5_work_begin(0, 0);
    m5_checkpoint(0, 0);
#endif

    #pragma omp parallel for
    for(int i = 0 ; i < N ; i++)
    {
         float lhs = vls[i].a[0];
         vls[i].a[0] =  lhs + spmv(R, val, idx, x, ptr, y);
    }
       

    // end of ROI
    //
#ifdef GEM5
    m5_work_end(0, 0);
#endif

    auto mm_end = std :: chrono :: high_resolution_clock :: now();

    // Free the memory allocated.
    //
    delete data_A;
    delete data_B;
    delete data_C;
    delete A;
    delete B;
    delete C_M;

    // We take a note of the final wall clock time before printing the final
    // statistics.
    //
    auto prog_end = std :: chrono :: high_resolution_clock :: now();
    std :: chrono :: duration<double> prog_elapsed = std :: chrono :: 
        duration_cast<std :: chrono :: duration<double>>(prog_end - prog_start);
    std :: chrono :: duration<double> mm_elapsed = std :: chrono :: 
        duration_cast<std :: chrono :: duration<double>>(mm_end - mm_start);

    // Printing statistics at the end of the program.
    //
    std :: cout << "Printing Statistics :: Wall Clock Time" << std :: endl;
    std :: cout << "======================================" << std :: endl; 
    std :: cout << "Program: " << prog_elapsed.count() << " s" << std :: endl;
    std :: cout << "Matrix Multiply: " << mm_elapsed.count() << " s" << std :: endl;
    std :: cout << std :: endl;

    return 1;
}