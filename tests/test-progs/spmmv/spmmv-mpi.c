#include "dataset1.h"
#include <omp.h>
#include <mpi.h>
#include <stdio.h>

void spmv(int r, const double* val, const int* idx, const double* x,
          const int* ptr, double* y)
{
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
  }
}

//--------------------------------------------------------------------------
// Main

int main(int argc, char* argv[])
{
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  double y[R];

  // Assuming the total iterations are divisible by the number of processes
  int iterations_per_process = 10000 / size;
  int start = rank * iterations_per_process;
  int end = start + iterations_per_process;

  for (int i = start; i < end; i++) {
    if (i % 1000 == 0) {
      printf("Process %d is at iteration %d\n", rank, i);
    }
    spmv(R, val, idx, x, ptr, y);
  }

  MPI_Finalize();
  return 0;
}