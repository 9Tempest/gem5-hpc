python3 run_gem5_base.py --cmd ./gather_contention --output gather_contention_dir --fast &
python3 run_gem5_base.py --cmd ./gather_contention_more --output gather_contention_more_dir --fast &
python3 run_gem5_base.py --cmd ./gather_transform --output gather_transform_dir --fast &
python3 run_gem5_base.py --cmd ./gather_tile --output gather_tile_dir --fast &
python3 run_gem5_base_smaller_cache.py --cmd ./gather_contention --output gather_contention_sm --fast &
python3 run_gem5_base_smaller_cache.py --cmd ./gather_contention_more --output gather_contention_more_dir_sm --fast &
python3 run_gem5_base_smaller_cache.py --cmd ./gather_tile --output gather_tile_dir_sm --fast &
