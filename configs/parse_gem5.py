import os
DATA_DIR = "/data3/arkhadem/gem5-hpc/results"

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

num_cores = {"gather": 4,
            "scatter": 1,
            "rmw": 1,
            "gather_scatter": 1,
            "gather_rmw": 1,
            "gather_rmw_cond": 1,
            "gather_rmw_directrangeloop_cond": 1,
            "gather_rmw_indirectrangeloop_cond": 1,
            "gather_rmw_cond_indirectrangeloop_cond": 1,
            "gather_rmw_indirectcond_indirectrangeloop_indirectcond": 1}

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

all_maa_indirect_cycles = ["Fill", "Drain", "Build"]
all_cache_stats = {"Avg-SPD-Latency": "system.switch_cpus0.lsq0.loadToUse_0::mean",
                   "L1-SPD-Hits": "system.cpu0.dcache.demandHits_0::switch_cpus0.data",
                   "L1-SPD-Misses": "system.cpu0.dcache.demandMisses_0::switch_cpus0.data",
                   "M1-SPD-Hits": "system.cpu0.dcache.demandMshrHits_0::switch_cpus0.data",
                   "M1-SPD-Misses": "system.cpu0.dcache.demandMshrMisses_0::switch_cpus0.data",
                   "L2-SPD-Hits": "system.cpu0.l2cache.demandHits_0::switch_cpus0.data",
                   "L2-SPD-Misses": "system.cpu0.l2cache.demandMisses_0::switch_cpus0.data",
                   "M2-SPD-Hits": "system.cpu0.l2cache.demandMshrHits_0::switch_cpus0.data",
                   "M2-SPD-Misses": "system.cpu0.l2cache.demandMshrMisses_0::switch_cpus0.data",
                   "L3-SPD-Hits": "system.l3.demandHits_0::switch_cpus0.data",
                   "L3-SPD-Misses": "system.l3.demandMisses_0::switch_cpus0.data",
                   "M3-SPD-Hits": "system.l3.demandMshrHits_0::switch_cpus0.data",
                   "M3-SPD-Misses": "system.l3.demandMshrMisses_0::switch_cpus0.data",
                   "Avg-Latency": "system.switch_cpus0.lsq0.loadToUse_T::mean",
                   "L1-Hits": "system.cpu0.dcache.demandHits_T::switch_cpus0.data",
                   "L1-Misses": "system.cpu0.dcache.demandMisses_T::switch_cpus0.data",
                   "M1-Hits": "system.cpu0.dcache.demandMshrHits_T::switch_cpus0.data",
                   "M1-Misses": "system.cpu0.dcache.demandMshrMisses_T::switch_cpus0.data",
                   "L2-Hits": "system.cpu0.l2cache.demandHits_T::switch_cpus0.data",
                   "L2-Misses": "system.cpu0.l2cache.demandMisses_T::switch_cpus0.data",
                   "M2-Hits": "system.cpu0.l2cache.demandMshrHits_T::switch_cpus0.data",
                   "M2-Misses": "system.cpu0.l2cache.demandMshrMisses_T::switch_cpus0.data",
                   "L3-Hits": "system.l3.demandHits_T::switch_cpus0.data",
                   "L3-Misses": "system.l3.demandMisses_T::switch_cpus0.data",
                   "M3-Hits": "system.l3.demandMshrHits_T::switch_cpus0.data",
                   "M3-Misses": "system.l3.demandMshrMisses_T::switch_cpus0.data"}

all_instruction_types = {"LDFP": "commitStats0.committedInstType::FloatMemRead",
                         "STFP": "commitStats0.committedInstType::FloatMemWrite",
                         "VFP": "commitStats0.committedInstType::SIMDFloat",
                         "SFP": "commitStats0.committedInstType::Float",
                         "SINT": "commitStats0.committedInstType::Int",
                         "VINT": "commitStats0.committedInstType::SIMD",
                         "LDINT": "commitStats0.committedInstType::MemRead",
                         "STINT": "commitStats0.committedInstType::MemWrite",
                         "VEC": "commitStats0.committedInstType::Vector",
                         "Total": "commitStats0.committedInstType::total"}

SPD_load_latencies = ""

all_tiles = [1024, 2048, 4096, 8192, 16384]
all_tiles_str = ["1K", "2K", "4K", "8K", "16K"]
all_sizes = [2000000] #, ]
all_sizes_str = ["2M"] #, ""]
all_distances = [256, 1024, 4096, 16384, 65536, 262144, 1048576, 4194304]
all_distances_str = ["256", "1K", "4K", "16K", "64K", "256K", "1M", "4M"]
all_modes = ["BASE", "MAA", "DMP"]

def parse_gem5_stats(stats, mode):
    cycles = 0
    maa_cycles = {}
    maa_indirect_cycles = {}
    cache_stats = {}
    instruction_types = {}
    for maa_cycle in all_maa_cycles:
        maa_cycles[maa_cycle] = 0
    for maa_indirect_cycle in all_maa_indirect_cycles:
        maa_indirect_cycles[maa_indirect_cycle] = 0
    for cache_stat in all_cache_stats.keys():
        cache_stats[cache_stat] = 0
    for instruction_type in all_instruction_types.keys():
        instruction_types[instruction_type] = 0

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
                found = False
                if "simTicks" == words[0]:
                    cycles = int(words[1]) / 500
                    continue
                for cache_stat in all_cache_stats.keys():
                    if all_cache_stats[cache_stat] == words[0]:
                        try:
                            cache_stats[cache_stat] = int(words[1])
                        except:
                            cache_stats[cache_stat] = float(words[1])
                        found = True
                        break
                if found:
                    continue
                if "system.switch_cpus" in words[0]:
                    for instruction_type in all_instruction_types.keys():
                        if all_instruction_types[instruction_type] in words[0]:
                            instruction_types[instruction_type] += int(words[1])
                            found = True
                            break
                if found:
                    continue
                if mode == "MAA":
                    if "system.maa.cycles" == words[0]:
                        maa_cycles["Total"] = int(words[1])
                        continue
                    for maa_cycle in all_maa_cycles:
                        if f"system.maa.cycles_{maa_cycle}" == words[0]:
                            maa_cycles[maa_cycle] = int(words[1])
                            found = True
                            break
                    if found:
                        continue
                    for maa_indirect_cycle in all_maa_indirect_cycles:
                        if f"system.maa.I0_IND_Cycles{maa_indirect_cycle}" == words[0]:
                            maa_indirect_cycles[maa_indirect_cycle] = int(words[1])
                            break
    # else:
    #     print(f"File not found: {stats}")

    return cycles, maa_cycles, maa_indirect_cycles, cache_stats, instruction_types

def parse_ramulator_stats(stats):
    DRAM_RD = 0
    DRAM_WR = 0
    DRAM_ACT = 0
    DRAM_RD_BW = 0
    DRAM_WR_BW = 0
    DRAM_total_BW = 0
    DRAM_RB_hitrate = 0
    DRAM_CTRL_occ = 0

    if os.path.exists(stats) == True:    
        cycles = 0
        num_reads = []
        num_writes = []
        num_acts = []
        avg_occupancy = []
        with open(stats, "r") as f:
            lines = f.readlines()
            test_started = False
            for line in lines:
                if "id: Channel 0" in line:
                    if test_started:
                        break
                    test_started = True
                    continue
                words = line.split(" ")
                words = [word for word in words if word != ""]
                for word, word_id in zip(words, range(len(words))):
                    if word == "#":
                        words = words[:word_id]
                        break
                if "memory_system_cycles" in line:
                    assert cycles == 0
                    cycles = int(words[-1])
                    continue
                if "num_RD_commands_T" in line:
                    num_reads.append(int(words[-1]))
                    continue
                if "num_WR_commands_T" in line:
                    num_writes.append(int(words[-1]))
                    continue
                if "avg_queue_occupancy_T" in line:
                    avg_occupancy.append(float(words[-1]))
                    continue
                if "num_ACT_commands_T" in line:
                    num_acts.append(int(words[-1]))
                    continue
        DRAM_RD = sum(num_reads)
        DRAM_WR = sum(num_writes)
        DRAM_ACT = sum(num_acts)
        DRAM_RD_BW = (DRAM_RD * 64)  / (cycles * 625)
        DRAM_RD_BW *= 1e12 # 625 is in PS
        DRAM_RD_BW /= (1024 * 1024 * 1024) # BW is in GB/s
        DRAM_WR_BW = (DRAM_WR * 64)  / (cycles * 625)
        DRAM_WR_BW *= 1e12 # 625 is in PS
        DRAM_WR_BW /= (1024 * 1024 * 1024) # BW is in GB/s
        DRAM_total_BW = DRAM_RD_BW + DRAM_WR_BW
        DRAM_RB_hitrate = 100.00 - (DRAM_ACT * 100.00 / (DRAM_RD + DRAM_WR))
        assert DRAM_RB_hitrate >= 0, f"DRAM_RB_hitrate: 100 - {DRAM_ACT * 100.00} / {DRAM_RD + DRAM_WR} == {DRAM_RB_hitrate}"
        DRAM_CTRL_occ = sum(avg_occupancy) / len(avg_occupancy)

    return DRAM_RD, DRAM_WR, DRAM_ACT, DRAM_RD_BW, DRAM_WR_BW, DRAM_total_BW, DRAM_RB_hitrate, DRAM_CTRL_occ

print("K,S,D,T,M,cycles", end=",")
for maa_cycle in all_maa_cycles:
    print(f"{maa_cycle}", end=",")
for maa_indirect_cycle in all_maa_indirect_cycles:
    print(f"IND_{maa_indirect_cycle}", end=",")
for cache_stat in all_cache_stats.keys():
    print(f"{cache_stat}", end=",")
for instruction_type in all_instruction_types.keys():
    print(f"{instruction_type}", end=",")
print("LSQ-LD-OCC", end=",")
print("DRAM-RD,DRAM-WR,DRAM-ACT,DRAM-RD-BW,DRAM-WR-BW,DRAM-total-BW,DRAM-RB-hitrate,DRAM-CTRL-occ", end=",")
print()

MAA_BASE_Speedup = {}
for kernel in all_kernels:
    MAA_BASE_Speedup[kernel] = {}
    for tile_size, tile_size_str in zip(all_tiles, all_tiles_str):
        MAA_BASE_Speedup[kernel][tile_size] = {}
        for size, size_str in zip(all_sizes, all_sizes_str):
            for distance, distance_str in zip(all_distances, all_distances_str):
                all_cycles = {}
                for mode in all_modes:
                    stats = f"{DATA_DIR}/{kernel}/M{mode}/D{distance_str}/T{tile_size_str}/S{size_str}/FP32/stats.txt"
                    cycles, maa_cycles, maa_indirect_cycles, cache_stats, instruction_types = parse_gem5_stats(stats, mode)
                    logs = f"{DATA_DIR}/{kernel}/M{mode}/D{distance_str}/T{tile_size_str}/S{size_str}/FP32/logs.txt"
                    DRAM_RD, DRAM_WR, DRAM_ACT, DRAM_RD_BW, DRAM_WR_BW, DRAM_total_BW, DRAM_RB_hitrate, DRAM_CTRL_occ = parse_ramulator_stats(logs)
                    all_cycles[mode] = cycles
                    print(f"{kernel},{size_str},{distance_str},{tile_size_str},{mode},{cycles}", end=",")
                    for maa_cycle in all_maa_cycles:
                        print(maa_cycles[maa_cycle], end=",")
                    for maa_indirect_cycle in all_maa_indirect_cycles:
                        print(maa_indirect_cycles[maa_indirect_cycle], end=",")
                    for cache_stat in all_cache_stats.keys():
                        print(cache_stats[cache_stat], end=",")
                    for instruction_type in all_instruction_types.keys():
                        print(instruction_types[instruction_type], end=",")
                    print(((instruction_types["LDINT"] + instruction_types["LDFP"]) * cache_stats["Avg-Latency"]) / cycles, end=",")
                    print(f"{DRAM_RD},{DRAM_WR},{DRAM_ACT},{DRAM_RD_BW},{DRAM_WR_BW},{DRAM_total_BW},{DRAM_RB_hitrate},{DRAM_CTRL_occ}", end=",")
                    print()
                if all_cycles["MAA"] == 0 or "BASE" not in all_modes:
                    MAA_BASE_Speedup[kernel][tile_size][distance] = 0
                else:
                    MAA_BASE_Speedup[kernel][tile_size][distance] = all_cycles["BASE"] / all_cycles["MAA"]

if "BASE" in all_modes:
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