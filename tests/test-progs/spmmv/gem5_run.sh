if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <output-dir>"
    exit 1
fi

export OMP_PROC_BIND=false; export OMP_NUM_THREADS=4;
/data3/gem5/build/X86/gem5.opt  /data3/gem5/configs/deprecated/example/se.py \
    --cpu-type O3CPU -n 4 --mem-size '2GB' --sys-clock '3GHz'\
  --caches --l1d_size=64kB --l1d_assoc=4 --l1i_size=32kB --l1i_assoc=4 --l2cache --l2_size=1MB --l2_assoc=16 --num-l2caches=4 --l3_size=8MB --cacheline_size=64 \
  --mem-type HBM_2000_4H_1x64 --mem-channels 2 --l1d-hwp-type StridePrefetcher --l2-hwp-type StridePrefetcher \
 --cmd ./spmmv-omp -r 1

mv m5out/stats.txt $1
python3 /data3/gem5/util/extract_stats.py  $1/stats.txt $1/stats.json