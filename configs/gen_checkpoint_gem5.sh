# --debug-flags=Exec

export OMP_PROC_BIND=false; export OMP_NUM_THREADS=4; ~/gem5-hpc/build/X86/gem5.opt --outdir=$1 ~/gem5-hpc/configs/deprecated/example/se.py --cpu-type AtomicSimpleCPU -n 4 --mem-size '16GB' --cmd $2