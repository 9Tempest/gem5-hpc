# Makefile for spmmv-mpi and spmmv-omp

CC = g++
MPI_CC = mpicc

GEM5_INCLUDE =  /home/lukezhuz/gem5-hpc/include/
GEM5_LIB =  /home/lukezhuz/gem5-hpc/util/m5/build/x86/out
M5OP_SRC =  /home/lukezhuz/gem5-hpc/util/m5/build/x86/abi/x86/m5op.S

CFLAGS = -I$(GEM5_INCLUDE) -I /home/lukezhuz/gem5-hpc/util/m5/src/ -L$(GEM5_LIB) -Wl,-Bstatic -L$(GEM5_LIB) -Wl,-Bdynamic -ldl  -O1 -DGEM5
OMP_FLAGS = -fopenmp

all: gather_transform gather_contention  gather_tile gather_kernel_contention_128 gather_kernel_contention_1280 gather_contention_more

gather_tile: gather_kernel_tile.cc $(M5OP_SRC)
	$(CC) $(CFLAGS) $^ -o $@

gather_contention: gather_kernel_contention_base.cc $(M5OP_SRC)
	$(CC) $(CFLAGS) $^ -o $@

gather_contention_more: gather_kernel_contention_more.cc $(M5OP_SRC)
	$(CC) $(CFLAGS) $^ -o $@

gather_kernel_contention_128: gather_kernel_contention_128.cc $(M5OP_SRC)
	$(CC) $(CFLAGS) $^ -o $@

gather_kernel_contention_1280: gather_kernel_contention_1280.cc $(M5OP_SRC)
	$(CC) $(CFLAGS) $^ -o $@

gather_transform: gather_kernel_one_transform.cc $(M5OP_SRC)
	$(CC) $(CFLAGS) $^ -o $@



clean:
	rm -f gather_contention gather_contention_ckpt gather_transform