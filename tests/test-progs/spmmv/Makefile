# Makefile for spmmv-mpi and spmmv-omp

CC = g++
MPI_CC = mpicc

GEM5_INCLUDE = /home/arkhadem/gem5-hpc/include/
GEM5_LIB = /home/arkhadem/gem5-hpc/util/m5/build/x86/out
M5OP_SRC = /home/arkhadem/gem5-hpc/util/m5/build/x86/abi/x86/m5op.S

CFLAGS = -I$(GEM5_INCLUDE) -I/home/arkhadem/gem5-hpc/util/m5/src/ -L$(GEM5_LIB) -Wl,-Bstatic -L$(GEM5_LIB) -Wl,-Bdynamic -ldl  -O1
OMP_FLAGS = -fopenmp
MPI_FLAGS = -static

all: mpi omp

mpi: spmmv-mpi
omp: spmmv-omp

spmmv-mpi: spmmv-mpi.c $(M5OP_SRC)
	$(MPI_CC) $(CFLAGS) $(MPI_FLAGS) $^ -o $@

spmmv-omp: spmmv-omp.cc $(M5OP_SRC)
	$(CC) $(CFLAGS) $(OMP_FLAGS) $^ -o $@

clean:
	rm -f spmmv-mpi spmmv-omp
