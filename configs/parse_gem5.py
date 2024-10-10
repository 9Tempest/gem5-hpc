import os
DATA_DIR = "/data1/arkhadem/gem5-hpc/results_complete_before_RT_optimization"

all_kernels =   ["gather",
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
                "IDLE",
                "Total"]

all_tiles = [1024, 2048, 4096, 8192, 16384]
all_tiles_str = ["1K", "2K", "4K", "8K", "16K"]
all_sizes = [200000] #, 2000000]
all_sizes_str = ["200K"] #, "2M"]
all_distances = [256, 1024, 4096, 16384, 65536, 262144, 1048576, 4194304]
all_distances_str = ["256", "1K", "4K", "16K", "64K", "256K", "1M", "4M"]

def parse_stats(stats, mode):
    cycles = 0
    maa_cycles = {}
    for maa_cycle in all_maa_cycles:
        maa_cycles[maa_cycle] = 0

    if os.path.exists(stats):    
        with open(stats, "r") as f:
            lines = f.readlines()
            test_started = False
            for line in lines:
                if "Begin Simulation Statistics" in line:
                    if test_started:
                        break
                    test_started = True
                    continue
                words = line.split(" ")
                words = [word for word in words if word != ""]
                if "simTicks" == words[0]:
                    cycles = int(words[1]) / 500
                    continue
                if mode == "MAA":
                    if "system.maa.cycles" == words[0]:
                        maa_cycles["Total"] = int(words[1])
                        continue
                    for maa_cycle in all_maa_cycles:
                        if f"system.maa.cycles_{maa_cycle}" == words[0]:
                            maa_cycles[maa_cycle] = int(words[1])
                            break
    else:
        print(f"File not found: {stats}")

    return cycles, maa_cycles

print("K,S,D,T,M,cycles,", end="")
for maa_cycle in all_maa_cycles:
    print(f"{maa_cycle},", end="")
print()

MAA_BASE_Speedup = {}
for kernel in all_kernels:
    MAA_BASE_Speedup[kernel] = {}
    for tile_size, tile_size_str in zip(all_tiles, all_tiles_str):
        MAA_BASE_Speedup[kernel][tile_size] = {}
        for size, size_str in zip(all_sizes, all_sizes_str):
            for distance, distance_str in zip(all_distances, all_distances_str):
                all_cycles = {}
                for mode in ["BASE", "MAA", "DMP"]:
                    stats = f"{DATA_DIR}/{kernel}/M{mode}/D{distance_str}/T{tile_size_str}/S{size_str}/FP32/stats.txt"
                    cycles, maa_cycles = parse_stats(stats, mode)
                    all_cycles[mode] = cycles
                    print(f"{kernel},{size_str},{distance_str},{tile_size_str},{mode},{cycles},", end="")
                    for maa_cycle in all_maa_cycles:
                        print(f"{maa_cycles[maa_cycle]},", end="")
                    print()
                MAA_BASE_Speedup[kernel][tile_size][distance] = all_cycles["BASE"] / all_cycles["MAA"]

print("Speedup")
for kernel in all_kernels:
    print(kernel, end=",")
    for tile_size_str in all_tiles_str:
        print(tile_size_str, end=",")
    print()
    for distance, distance_str in zip(all_distances, all_distances_str):
        print(f"{distance_str},", end="")
        for tile_size in all_tiles:
            print(MAA_BASE_Speedup[kernel][tile_size][distance], end=",")
        print()