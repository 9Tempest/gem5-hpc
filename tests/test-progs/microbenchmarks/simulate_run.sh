python3 run_gem5_base.py --cmd ./gather_contention --output gather_contention_dir --fast &
python3 run_gem5_base.py --cmd ./gather_kernel_contention_128 --output gather_kernel_contention_128_dir --fast &
python3 run_gem5_base.py --cmd ./gather_kernel_contention_1280 --output gather_kernel_contention_1280_dir --fast &
python3 run_gem5_base_smaller_cache.py --cmd ./gather_contention --output gather_contention_sm --fast &
python3 run_gem5_base_smaller_cache.py --cmd ./gather_kernel_contention_128 --output gather_kernel_contention_128_sm --fast &
python3 run_gem5_base_smaller_cache.py --cmd ./gather_kernel_contention_1280 --output gather_kernel_contention_1280_sm --fast 