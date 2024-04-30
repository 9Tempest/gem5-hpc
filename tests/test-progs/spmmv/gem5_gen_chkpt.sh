
export OMP_PROC_BIND=false; export OMP_NUM_THREADS=4;
/home/arkhadem/gem5-hpc/build/X86/gem5.opt  /home/arkhadem/gem5-hpc/configs/deprecated/example/se.py --cpu-type AtomicSimpleCPU -n 4 --mem-size '16GB' --sys-clock '3GHz' --cmd ./spmmv-omp