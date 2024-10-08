import os
DATA_DIR = "/data1/arkhadem/gem5-hpc"

all_tests = ["gather",
            "scatter",
            "rmw",
            "gather_scatter",
            "gather_rmw",
            "gather_rmw_cond",
            "gather_rmw_directrangeloop_cond",
            "gather_rmw_indirectrangeloop_cond",
            "gather_rmw_cond_indirectrangeloop_cond",
            "gather_rmw_indirectcond_indirectrangeloop_indirectcond"]

all_maa_cycles = ["INDRD",
                "INDWR",
                "INDRMW",
                "STRRD",
                "RANGE",
                "ALUS",
                "ALUV",
                "INV",
                "IDLE"]

all_tiles = [1024, 2048, 4096, 8192, 16384]
all_tiles_str = ["1K", "2K", "4K", "8K", "16K"]
all_sizes = [20000, 2000000]
all_sizes_str = ["20K", "2M"]
all_distances = [256, 1024, 4096, 16384, 65536, 262144, 1048576, 4194304]
all_distances_str = ["256", "1K", "4K", "16K", "64K", "256K", "1M", "4M"]

def parse_stats(stats, mode):
    # check if stats file exists
    if not os.path.exists(stats):
        # print(f"Stats file {stats} does not exist.")
        return None, None, None
    
    with open(stats, "r") as f:
        lines = f.readlines()
        test_id = -1
        cycles = {}
        maa_total_cycles = {}
        maa_cycles = {}
        for line in lines:
            if "Begin Simulation Statistics" in line:
                test_id += 1
                if test_id == len(all_tests):
                    break
                else:
                    maa_cycles[all_tests[test_id]] = {}
                    for maa_cycle in all_maa_cycles:
                        maa_cycles[all_tests[test_id]][maa_cycle] = 0
                    continue            
            words = line.split(" ")
            # remove "" strings
            words = [word for word in words if word != ""]
            if "simTicks" == words[0]:
                cycles[all_tests[test_id]] = int(words[1]) / 500
                continue
            if mode == "MAA":
                if "system.maa.cycles" == words[0]:
                    maa_total_cycles[all_tests[test_id]] = int(words[1])
                    continue
                for maa_cycle in all_maa_cycles:
                    if f"system.maa.cycles_{maa_cycle}" == words[0]:
                        maa_cycles[all_tests[test_id]][maa_cycle] = int(words[1])
                        break
    
    if len(cycles) != len(all_tests):
        print(f"Stats file {stats} is incomplete.")
        return None, None, None

    return cycles, maa_total_cycles, maa_cycles

print("S,D,T,test,BASE_cycles,DMP_cycles,MAA_cycles,MAA_total_cycles,", end="")
for maa_cycle in all_maa_cycles:
    print(f"{maa_cycle}_cycles,", end="")
print()

for tile_size, tile_size_str in zip(all_tiles, all_tiles_str):
    for size, size_str in zip(all_sizes, all_sizes_str):
        for distance, distance_str in zip(all_distances, all_distances_str):
            modes_cycles = {}
            maa_total_cycles = None
            maa_cycles = None
            for mode in ["BASE", "MAA", "DMP"]:
                new_mode = mode
                if mode == "DMP":
                    new_mode = f"BASE"

                stats = f"{DATA_DIR}/results/M{mode}/D{distance_str}/T{tile_size_str}/S{size_str}/FP32/stats.txt"
                modes_cycles[mode], maa_total_cycles_tmp, maa_cycles_tmp = parse_stats(stats, mode)
                if mode == "MAA":
                    maa_total_cycles = maa_total_cycles_tmp
                    maa_cycles = maa_cycles_tmp
            for test in all_tests:
                base_c = modes_cycles['BASE'][test] if modes_cycles['BASE'] != None else 0
                dmp_c = modes_cycles['DMP'][test] if modes_cycles['DMP'] != None else 0
                maa_c = modes_cycles['MAA'][test] if modes_cycles['MAA'] != None else 0
                maa_total_c = maa_total_cycles[test] if maa_total_cycles != None else 0
                print(f"{size_str},{distance_str},{tile_size_str},{test},{base_c},{dmp_c},{maa_c},{maa_total_c},", end="")
                for maa_cycle in all_maa_cycles:
                    if maa_cycles != None:
                        print(f"{maa_cycles[test][maa_cycle]},", end="")
                    else:
                        print("0,", end="")
                print()