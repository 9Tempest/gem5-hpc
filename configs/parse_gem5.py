import os
DATA_DIR = "/data1/arkhadem/gem5-hpc/tests"
RSLT_DIR = f"{DATA_DIR}/results"

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

all_maa_indirect_cycles = ["Fill", "Drain", "Build", "Request"]
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
                   "Avg-A1-Latency": "system.switch_cpus0.lsq0.loadToUse_6::mean",
                   "L1-A1-Hits": "system.cpu0.dcache.demandHits_6::switch_cpus0.data",
                   "L1-A1-Misses": "system.cpu0.dcache.demandMisses_6::switch_cpus0.data",
                   "M1-A1-Hits": "system.cpu0.dcache.demandMshrHits_6::switch_cpus0.data",
                   "M1-A1-Misses": "system.cpu0.dcache.demandMshrMisses_6::switch_cpus0.data",
                   "L2-A1-Hits": "system.cpu0.l2cache.demandHits_6::switch_cpus0.data",
                   "L2-A1-Misses": "system.cpu0.l2cache.demandMisses_6::switch_cpus0.data",
                   "M2-A1-Hits": "system.cpu0.l2cache.demandMshrHits_6::switch_cpus0.data",
                   "M2-A1-Misses": "system.cpu0.l2cache.demandMshrMisses_6::switch_cpus0.data",
                   "L3-A1-Hits": "system.l3.demandHits_6::switch_cpus0.data",
                   "L3-A1-Misses": "system.l3.demandMisses_6::switch_cpus0.data",
                   "M3-A1-Hits": "system.l3.demandMshrHits_6::switch_cpus0.data",
                   "M3-A1-Misses": "system.l3.demandMshrMisses_6::switch_cpus0.data",
                   "Avg-A2-Latency": "system.switch_cpus0.lsq0.loadToUse_7::mean",
                   "L1-A2-Hits": "system.cpu0.dcache.demandHits_7::switch_cpus0.data",
                   "L1-A2-Misses": "system.cpu0.dcache.demandMisses_7::switch_cpus0.data",
                   "M1-A2-Hits": "system.cpu0.dcache.demandMshrHits_7::switch_cpus0.data",
                   "M1-A2-Misses": "system.cpu0.dcache.demandMshrMisses_7::switch_cpus0.data",
                   "L2-A2-Hits": "system.cpu0.l2cache.demandHits_7::switch_cpus0.data",
                   "L2-A2-Misses": "system.cpu0.l2cache.demandMisses_7::switch_cpus0.data",
                   "M2-A2-Hits": "system.cpu0.l2cache.demandMshrHits_7::switch_cpus0.data",
                   "M2-A2-Misses": "system.cpu0.l2cache.demandMshrMisses_7::switch_cpus0.data",
                   "L3-A2-Hits": "system.l3.demandHits_7::switch_cpus0.data",
                   "L3-A2-Misses": "system.l3.demandMisses_7::switch_cpus0.data",
                   "M3-A2-Hits": "system.l3.demandMshrHits_7::switch_cpus0.data",
                   "M3-A2-Misses": "system.l3.demandMshrMisses_7::switch_cpus0.data",
                   "Avg-B-Latency": "system.switch_cpus0.lsq0.loadToUse_8::mean",
                   "L1-B-Hits": "system.cpu0.dcache.demandHits_8::switch_cpus0.data",
                   "L1-B-Misses": "system.cpu0.dcache.demandMisses_8::switch_cpus0.data",
                   "M1-B-Hits": "system.cpu0.dcache.demandMshrHits_8::switch_cpus0.data",
                   "M1-B-Misses": "system.cpu0.dcache.demandMshrMisses_8::switch_cpus0.data",
                   "L2-B-Hits": "system.cpu0.l2cache.demandHits_8::switch_cpus0.data",
                   "L2-B-Misses": "system.cpu0.l2cache.demandMisses_8::switch_cpus0.data",
                   "M2-B-Hits": "system.cpu0.l2cache.demandMshrHits_8::switch_cpus0.data",
                   "M2-B-Misses": "system.cpu0.l2cache.demandMshrMisses_8::switch_cpus0.data",
                   "L3-B-Hits": "system.l3.demandHits_8::switch_cpus0.data",
                   "L3-B-Misses": "system.l3.demandMisses_8::switch_cpus0.data",
                   "M3-B-Hits": "system.l3.demandMshrHits_8::switch_cpus0.data",
                   "M3-B-Misses": "system.l3.demandMshrMisses_8::switch_cpus0.data",
                   "Avg-IDX-Latency": "system.switch_cpus0.lsq0.loadToUse_9::mean",
                   "L1-IDX-Hits": "system.cpu0.dcache.demandHits_9::switch_cpus0.data",
                   "L1-IDX-Misses": "system.cpu0.dcache.demandMisses_9::switch_cpus0.data",
                   "M1-IDX-Hits": "system.cpu0.dcache.demandMshrHits_9::switch_cpus0.data",
                   "M1-IDX-Misses": "system.cpu0.dcache.demandMshrMisses_9::switch_cpus0.data",
                   "L2-IDX-Hits": "system.cpu0.l2cache.demandHits_9::switch_cpus0.data",
                   "L2-IDX-Misses": "system.cpu0.l2cache.demandMisses_9::switch_cpus0.data",
                   "M2-IDX-Hits": "system.cpu0.l2cache.demandMshrHits_9::switch_cpus0.data",
                   "M2-IDX-Misses": "system.cpu0.l2cache.demandMshrMisses_9::switch_cpus0.data",
                   "L3-IDX-Hits": "system.l3.demandHits_9::switch_cpus0.data",
                   "L3-IDX-Misses": "system.l3.demandMisses_9::switch_cpus0.data",
                   "M3-IDX-Hits": "system.l3.demandMshrHits_9::switch_cpus0.data",
                   "M3-IDX-Misses": "system.l3.demandMshrMisses_9::switch_cpus0.data",
                   "Avg-BND-Latency": "system.switch_cpus0.lsq0.loadToUse_10::mean",
                   "L1-BND-Hits": "system.cpu0.dcache.demandHits_10::switch_cpus0.data",
                   "L1-BND-Misses": "system.cpu0.dcache.demandMisses_10::switch_cpus0.data",
                   "M1-BND-Hits": "system.cpu0.dcache.demandMshrHits_10::switch_cpus0.data",
                   "M1-BND-Misses": "system.cpu0.dcache.demandMshrMisses_10::switch_cpus0.data",
                   "L2-BND-Hits": "system.cpu0.l2cache.demandHits_10::switch_cpus0.data",
                   "L2-BND-Misses": "system.cpu0.l2cache.demandMisses_10::switch_cpus0.data",
                   "M2-BND-Hits": "system.cpu0.l2cache.demandMshrHits_10::switch_cpus0.data",
                   "M2-BND-Misses": "system.cpu0.l2cache.demandMshrMisses_10::switch_cpus0.data",
                   "L3-BND-Hits": "system.l3.demandHits_10::switch_cpus0.data",
                   "L3-BND-Misses": "system.l3.demandMisses_10::switch_cpus0.data",
                   "M3-BND-Hits": "system.l3.demandMshrHits_10::switch_cpus0.data",
                   "M3-BND-Misses": "system.l3.demandMshrMisses_10::switch_cpus0.data",
                   "Avg-COND-Latency": "system.switch_cpus0.lsq0.loadToUse_11::mean",
                   "L1-COND-Hits": "system.cpu0.dcache.demandHits_11::switch_cpus0.data",
                   "L1-COND-Misses": "system.cpu0.dcache.demandMisses_11::switch_cpus0.data",
                   "M1-COND-Hits": "system.cpu0.dcache.demandMshrHits_11::switch_cpus0.data",
                   "M1-COND-Misses": "system.cpu0.dcache.demandMshrMisses_11::switch_cpus0.data",
                   "L2-COND-Hits": "system.cpu0.l2cache.demandHits_11::switch_cpus0.data",
                   "L2-COND-Misses": "system.cpu0.l2cache.demandMisses_11::switch_cpus0.data",
                   "M2-COND-Hits": "system.cpu0.l2cache.demandMshrHits_11::switch_cpus0.data",
                   "M2-COND-Misses": "system.cpu0.l2cache.demandMshrMisses_11::switch_cpus0.data",
                   "L3-COND-Hits": "system.l3.demandHits_11::switch_cpus0.data",
                   "L3-COND-Misses": "system.l3.demandMisses_11::switch_cpus0.data",
                   "M3-COND-Hits": "system.l3.demandMshrHits_11::switch_cpus0.data",
                   "M3-COND-Misses": "system.l3.demandMshrMisses_11::switch_cpus0.data",
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

def parse_gem5_stats(stats, mode, target_stats):
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
            current_num_tests = 0
            for line in lines:
                if "Begin Simulation Statistics" in line:
                    current_num_tests += 1
                    if current_num_tests > target_stats:
                        break
                    else:
                        continue
                if current_num_tests < target_stats:
                    continue
                words = line.split(" ")
                words = [word for word in words if word != ""]
                found = False
                if "simTicks" == words[0]:
                    cycles = int(words[1]) / 313
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
                if mode == "MAA" or mode == "maa":
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

def parse_ramulator_stats(stats, target_stats):
    # print(stats)
    DRAM_RD = 0
    DRAM_WR = 0
    DRAM_ACT = 0
    DRAM_RD_BW = 0
    DRAM_WR_BW = 0
    DRAM_total_BW = 0
    DRAM_RB_hitrate = 0
    DRAM_CTRL_occ = 0

    if os.path.exists(f"{stats}") == True:    
        COMMAND = f"cat {stats} | sed -n \'/Dumping ramulator/,$p\' > {stats}.ramulator"
        # print(COMMAND)
        os.system(COMMAND)
        cycles = 0
        num_channels = 2
        num_reads = [0 for _ in range(num_channels)]
        num_writes = [0 for _ in range(num_channels)]
        num_acts = [0 for _ in range(num_channels)]
        avg_occupancy = [0 for _ in range(num_channels)]
        with open(f"{stats}.ramulator", "r") as f:
            lines = f.readlines()
            current_num_tests = 0
            for line in lines:
                if "Dumping ramulator's stats" in line:
                    current_num_tests += 1
                    if current_num_tests > num_channels * target_stats:
                        break
                    else:
                        continue
                if current_num_tests < num_channels * target_stats - 1:
                    continue
                words = line.split(" ")
                words = [word for word in words if word != ""]
                for word, word_id in zip(words, range(len(words))):
                    if word == "#":
                        words = words[:word_id]
                        break
                if cycles == 0:
                    if "memory_system_ROI_cycles" in line:
                        cycles = int(words[-1])
                        # print(f"cycles = {cycles}")
                        continue                    
                if "num_RD_commands_T" in line:
                    channel = int(words[-2].split("_")[0].split("CH")[1])
                    if num_reads[channel] == 0:
                        num_reads[channel] = int(words[-1])
                        # print(f"num_reads[{channel}] = {num_reads[channel]}")
                    continue
                if "num_WR_commands_T" in line:
                    channel = int(words[-2].split("_")[0].split("CH")[1])
                    if num_writes[channel] == 0:
                        num_writes[channel] = int(words[-1])
                        # print(f"num_writes[{channel}] = {num_writes[channel]}")
                    continue
                if "avg_queue_occupancy_T" in line:
                    channel = int(words[-2].split("_")[0].split("CH")[1])
                    if avg_occupancy[channel] == 0:
                        avg_occupancy[channel] = float(words[-1])
                        # print(f"avg_occupancy[{channel}] = {avg_occupancy[channel]}")
                    continue
                if "num_ACT_commands_T" in line:
                    channel = int(words[-2].split("_")[0].split("CH")[1])
                    if num_acts[channel] == 0:
                        num_acts[channel] = int(words[-1])
                        # print(f"num_acts[{channel}] = {num_acts[channel]}")
                    continue
        if cycles != 0:
            # for channel in range(num_channels):
            #     if num_reads[channel] == 0 or num_writes[channel] == 0 or num_acts[channel] == 0 or avg_occupancy[channel] == 0:
            #         print(f"Error: {num_reads[channel]}, {num_writes[channel]}, {num_acts[channel]}, {avg_occupancy[channel]}")
            #         exit(-1)
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
            # assert DRAM_RB_hitrate >= 0, f"DRAM_RB_hitrate: 100 - {DRAM_ACT * 100.00} / {DRAM_RD + DRAM_WR} == {DRAM_RB_hitrate}"
            DRAM_CTRL_occ = sum(avg_occupancy) / len(avg_occupancy)
    # exit()
    return DRAM_RD, DRAM_WR, DRAM_ACT, DRAM_RD_BW, DRAM_WR_BW, DRAM_total_BW, DRAM_RB_hitrate, DRAM_CTRL_occ

print("K,S,D,M,cycles", end=",")
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

def get_print_results(directory, target_stats, kernel, size_str, distr, mode):
    stats = f"{directory}/stats.txt"
    cycles, maa_cycles, maa_indirect_cycles, cache_stats, instruction_types = parse_gem5_stats(stats, mode, target_stats)
    logs = f"{directory}/logs_run.txt"
    DRAM_RD, DRAM_WR, DRAM_ACT, DRAM_RD_BW, DRAM_WR_BW, DRAM_total_BW, DRAM_RB_hitrate, DRAM_CTRL_occ = parse_ramulator_stats(logs, target_stats)
    print(f"{kernel},{size_str},{distr},{mode},{cycles}", end=",")
    for maa_cycle in all_maa_cycles:
        print(maa_cycles[maa_cycle], end=",")
    for maa_indirect_cycle in all_maa_indirect_cycles:
        print(maa_indirect_cycles[maa_indirect_cycle], end=",")
    for cache_stat in all_cache_stats.keys():
        print(cache_stats[cache_stat], end=",")
    for instruction_type in all_instruction_types.keys():
        print(instruction_types[instruction_type], end=",")
    if cycles == 0:
        print(0, end=",")
    else:
        print(((instruction_types["LDINT"] + instruction_types["LDFP"]) * cache_stats["Avg-Latency"]) / cycles, end=",")
    print(f"{DRAM_RD},{DRAM_WR},{DRAM_ACT},{DRAM_RD_BW},{DRAM_WR_BW},{DRAM_total_BW},{DRAM_RB_hitrate},{DRAM_CTRL_occ}", end=",")
    print()

for kernel in ["gather", "rmw", "scatter", "gather_directrangeloop_indircond"]:
    test_dir = "CISC"
    for size, size_str in zip([65536], ["64K"]):
        for mode in ["BASE", "MAA"]:
            directory=f"{RSLT_DIR}/{kernel}/allhit/{size_str}_{mode}_new"
            get_print_results(directory, 2, kernel, size_str, "allhit", mode)
            directory=f"{RSLT_DIR}/{kernel}/allhitl3/{size_str}_{mode}_new"
            get_print_results(directory, 1, kernel, size_str, "allhitl3", mode)
            # for BAH in [0, 1]:
            #     for RBH in [0, 50, 100]: # [0, 25, 50, 75, 100]: # [0]:
            #         for CHH in [0, 1]: # [0]:
            #             if CHH == 1 and (BAH != 1 or RBH != 100):
            #                 continue
            #             for BGH in [0, 1]: # [0]:
            #                 if BGH == 1 and (BAH != 1 or RBH != 100 or CHH != 1):
            #                     continue
            #                 directory=f"{RSLT_DIR}/{kernel}/allmiss/BAH{BAH}/RBH{RBH}/CBH{CHH}/BGH{BGH}/{size_str}_{mode}_new"
            #                 get_print_results(directory, 1, kernel, size_str, f"allmiss_BAH{BAH}_RBH{RBH}_CBH{CHH}_BGH{BGH}", mode)

# print("M,L2,L3,cycles", end=",")
# for maa_cycle in all_maa_cycles:
#     print(f"{maa_cycle}", end=",")
# for maa_indirect_cycle in all_maa_indirect_cycles:
#     print(f"IND_{maa_indirect_cycle}", end=",")
# for cache_stat in all_cache_stats.keys():
#     print(f"{cache_stat}", end=",")
# for instruction_type in all_instruction_types.keys():
#     print(f"{instruction_type}", end=",")
# print("LSQ-LD-OCC", end=",")
# print("DRAM-RD,DRAM-WR,DRAM-ACT,DRAM-RD-BW,DRAM-WR-BW,DRAM-total-BW,DRAM-RB-hitrate,DRAM-CTRL-occ", end=",")
# print()

# for mode in ["maa", "base"]:
#     for l2 in ["l2", "nol2"]:
#         for l3 in ["l3", "nol3"]:
#             if mode == "base" and (l2 == "nol2" or l3 == "nol3"):
#                 continue
#             stats = f"tests_16T_{l2}_{l3}_{mode}/stats.txt"
#             cycles, maa_cycles, maa_indirect_cycles, cache_stats, instruction_types = parse_gem5_stats(stats, mode)
#             logs = f"tests_16T_{l2}_{l3}_{mode}/logs_run.txt"
#             DRAM_RD, DRAM_WR, DRAM_ACT, DRAM_RD_BW, DRAM_WR_BW, DRAM_total_BW, DRAM_RB_hitrate, DRAM_CTRL_occ = parse_ramulator_stats(logs)
#             print(f"{mode},{l2},{l3},{cycles}", end=",")
#             for maa_cycle in all_maa_cycles:
#                 print(maa_cycles[maa_cycle], end=",")
#             for maa_indirect_cycle in all_maa_indirect_cycles:
#                 print(maa_indirect_cycles[maa_indirect_cycle], end=",")
#             for cache_stat in all_cache_stats.keys():
#                 print(cache_stats[cache_stat], end=",")
#             for instruction_type in all_instruction_types.keys():
#                 print(instruction_types[instruction_type], end=",")
#             if cycles == 0:
#                 print(0, end=",")
#             else:
#                 print(((instruction_types["LDINT"] + instruction_types["LDFP"]) * cache_stats["Avg-Latency"]) / cycles, end=",")
#             print(f"{DRAM_RD},{DRAM_WR},{DRAM_ACT},{DRAM_RD_BW},{DRAM_WR_BW},{DRAM_total_BW},{DRAM_RB_hitrate},{DRAM_CTRL_occ}", end=",")
#             print()
# exit(-1)

# print("K,S,D,T,M,cycles", end=",")
# for maa_cycle in all_maa_cycles:
#     print(f"{maa_cycle}", end=",")
# for maa_indirect_cycle in all_maa_indirect_cycles:
#     print(f"IND_{maa_indirect_cycle}", end=",")
# for cache_stat in all_cache_stats.keys():
#     print(f"{cache_stat}", end=",")
# for instruction_type in all_instruction_types.keys():
#     print(f"{instruction_type}", end=",")
# print("LSQ-LD-OCC", end=",")
# print("DRAM-RD,DRAM-WR,DRAM-ACT,DRAM-RD-BW,DRAM-WR-BW,DRAM-total-BW,DRAM-RB-hitrate,DRAM-CTRL-occ", end=",")
# print()

# MAA_BASE_Speedup = {}
# for kernel in all_kernels:
#     MAA_BASE_Speedup[kernel] = {}
#     for tile_size, tile_size_str in zip(all_tiles, all_tiles_str):
#         MAA_BASE_Speedup[kernel][tile_size] = {}
#         for size, size_str in zip(all_sizes, all_sizes_str):
#             for distance, distance_str in zip(all_distances, all_distances_str):
#                 all_cycles = {}
#                 for mode in all_modes:
#                     stats = f"{DATA_DIR}/{kernel}/M{mode}/D{distance_str}/T{tile_size_str}/S{size_str}/FP32/stats.txt"
#                     cycles, maa_cycles, maa_indirect_cycles, cache_stats, instruction_types = parse_gem5_stats(stats, mode)
#                     logs = f"{DATA_DIR}/{kernel}/M{mode}/D{distance_str}/T{tile_size_str}/S{size_str}/FP32/logs.txt"
#                     DRAM_RD, DRAM_WR, DRAM_ACT, DRAM_RD_BW, DRAM_WR_BW, DRAM_total_BW, DRAM_RB_hitrate, DRAM_CTRL_occ = parse_ramulator_stats(logs)
#                     all_cycles[mode] = cycles
#                     print(f"{kernel},{size_str},{distance_str},{tile_size_str},{mode},{cycles}", end=",")
#                     for maa_cycle in all_maa_cycles:
#                         print(maa_cycles[maa_cycle], end=",")
#                     for maa_indirect_cycle in all_maa_indirect_cycles:
#                         print(maa_indirect_cycles[maa_indirect_cycle], end=",")
#                     for cache_stat in all_cache_stats.keys():
#                         print(cache_stats[cache_stat], end=",")
#                     for instruction_type in all_instruction_types.keys():
#                         print(instruction_types[instruction_type], end=",")
#                     print(((instruction_types["LDINT"] + instruction_types["LDFP"]) * cache_stats["Avg-Latency"]) / cycles, end=",")
#                     print(f"{DRAM_RD},{DRAM_WR},{DRAM_ACT},{DRAM_RD_BW},{DRAM_WR_BW},{DRAM_total_BW},{DRAM_RB_hitrate},{DRAM_CTRL_occ}", end=",")
#                     print()
#                 if all_cycles["MAA"] == 0 or "BASE" not in all_modes:
#                     MAA_BASE_Speedup[kernel][tile_size][distance] = 0
#                 else:
#                     MAA_BASE_Speedup[kernel][tile_size][distance] = all_cycles["BASE"] / all_cycles["MAA"]

# if "BASE" in all_modes:
#     print("Speedup")
#     for kernel in all_kernels:
#         print(kernel, end=",")
#         for tile_size_str in all_tiles_str:
#             print(tile_size_str, end=",")
#         print()
#         for distance, distance_str in zip(all_distances, all_distances_str):
#             print(f"{distance_str},", end="")
#             for tile_size in all_tiles:
#                 print(MAA_BASE_Speedup[kernel][tile_size][distance], end=",")
#             print()