import argparse
import os
from threading import Thread, Lock

parallelism = 24
GEM5_DIR = "/home/arkhadem/gem5-hpc"
DATA_DIR = "/data1/arkhadem/gem5-hpc"
LOG_DIR = "/data1/arkhadem/gem5-hpc/logs"

os.system("mkdir -p " + LOG_DIR)

tasks = []
lock = Lock()

def workerthread(my_tid):
    global lock
    global tasks
    task = None
    my_log_addr = f"{LOG_DIR}/logs_T{my_tid}.txt"
    os.system(f"rm {my_log_addr} 2>&1 > /dev/null; sleep 1;")
    while True:
        with lock:
            if len(tasks) == 0:
                task = None
            else:
                task = tasks.pop(0)
        if task == None:
            with open (my_log_addr, "a") as f:
                f.write("T[{}]: tasks finished, bye!\n".format(my_tid))
            print("T[{}]: tasks finished, bye!".format(my_tid))
            break
        else:
            with open (my_log_addr, "a") as f:
                f.write("T[{}]: executing a new task: {}\n".format(my_tid, task))
            print("T[{}]: executing a new task: {}".format(my_tid, task))
            os.system(f"{task} 2>&1 | tee -a {my_log_addr}")


cpu_type = "X86O3CPU"
core_num = 4
mem_size = "16GB"
sys_clock = "3.2GHz"
l1d_size = "32kB"
l1d_assoc = 8
l1d_hwp_type = "StridePrefetcher"
l1d_mshrs = 16
l1i_size = "32kB"
l1i_assoc = 8
l1i_hwp_type = "StridePrefetcher"
l1i_mshrs = 16
l2_size = "256kB"
l2_assoc = 4
l2_mshrs = 32
l3_mshrs = 256
mem_type = "Ramulator2"
ramulator_config = f"{GEM5_DIR}/ext/ramulator2/ramulator2/example_gem5_config.yaml"
mem_channels = 1
program_interval = 1000
debug_type = None
# debug_type = "LSQ,CacheAll,PseudoInst"
# debug_type = "O3CPUAll,CacheAll,PseudoInst"
# MemoryAccess,XBar,Cache,MAACpuPort,XBar,MemoryAccess,Cache,
# if sim_type == "MAA":
#     debug_type = "SPD,MAARangeFuser,MAAALU,MAAController,MAACachePort,MAAMemPort,MAAIndirect,MAAStream,MAAInvalidator"
    # debug_type = "MAACachePort,MAAIndirect,MAAStream,Cache"

def add_command_checkpoint(directory, command, options):
    COMMAND = f"rm -r {directory} 2>&1 > /dev/null; sleep 1; mkdir -p {directory}; sleep 1; "
    COMMAND += f"OMP_PROC_BIND=false OMP_NUM_THREADS=4 build/X86/gem5.fast "
    COMMAND += f"--outdir={directory} "
    COMMAND += f"{GEM5_DIR}/configs/deprecated/example/se.py "
    COMMAND += f"--cpu-type AtomicSimpleCPU -n 4 --mem-size \"16GB\" "
    COMMAND += f"--cmd {command} --options \"{options}\" "
    COMMAND += f"2>&1 "
    COMMAND += "| awk '{ print strftime(), $0; fflush() }' "
    COMMAND += f"| tee {directory}/logs.txt "
    tasks.append(COMMAND)

def add_command_run_MAA(directory, checkpoint, command, options, mode, tile_size):
    have_maa = False
    l2_hwp_type = "StridePrefetcher"
    l3_size = "8MB"
    l3_assoc = 16
    if mode == "DMP":
        l2_hwp_type = "DiffMatchingPrefetcher"
        l3_size = "10MB"
        l3_assoc = 20
    elif mode in ["MAA", "CMP"]:
        have_maa = True
    elif mode == "BASE":
        l3_size = "10MB"
        l3_assoc = 20
    else:
        raise ValueError("Unknown mode")

    COMMAND = f"OMP_PROC_BIND=false OMP_NUM_THREADS={core_num} {GEM5_DIR}/build/X86/gem5.opt "
    if debug_type != None:
        COMMAND += f"--debug-flags={debug_type} "
    COMMAND += f"--outdir={directory} "
    COMMAND += f"{GEM5_DIR}/configs/deprecated/example/se.py "
    COMMAND += f"--cpu-type {cpu_type} "
    COMMAND += f"-n {core_num} "
    COMMAND += f"--mem-size '{mem_size}' "
    COMMAND += f"--sys-clock '{sys_clock}' "
    COMMAND += f"--caches "
    COMMAND += f"--l1d_size={l1d_size} "
    COMMAND += f"--l1d_assoc={l1d_assoc} "
    COMMAND += f"--l1d-hwp-type={l1d_hwp_type} "
    COMMAND += f"--l1d_mshrs={l1d_mshrs} "
    COMMAND += f"--l1i_size={l1i_size} "
    COMMAND += f"--l1i_assoc={l1i_assoc} "
    COMMAND += f"--l1i-hwp-type={l1i_hwp_type} "
    COMMAND += f"--l1i_mshrs={l1i_mshrs} "
    COMMAND += f"--l2cache "
    COMMAND += f"--l2_size={l2_size} "
    COMMAND += f"--l2_assoc={l2_assoc} "
    COMMAND += f"--l2-hwp-type={l2_hwp_type} "
    if l2_hwp_type == "DiffMatchingPrefetcher":
        COMMAND += f"--dmp-notify l1 "
    COMMAND += f"--l2_mshrs={l2_mshrs} "
    COMMAND += f"--l3cache "
    COMMAND += f"--l3_size={l3_size} "
    COMMAND += f"--l3_assoc={l3_assoc} "
    COMMAND += f"--l3_mshrs={l3_mshrs} "
    COMMAND += "--cacheline_size=64 "
    COMMAND += f"--mem-type {mem_type} "
    COMMAND += f"--ramulator-config {ramulator_config} "
    COMMAND += f"--mem-channels {mem_channels} "
    if have_maa:
        COMMAND += "--maa "
        COMMAND += f"--maa_num_tile_elements {tile_size} "
    COMMAND += f"--cmd {command} "
    COMMAND += f"--options \"{options}\" "
    COMMAND += f"-r 1 "
    COMMAND += f"--prog-interval={program_interval} "
    COMMAND += f"2>&1 "
    COMMAND += "| awk '{ print strftime(), $0; fflush() }' "
    COMMAND += f"| tee {directory}/logs.txt "
    tasks.append(f"rm -r {directory} 2>&1 > /dev/null; sleep 1; mkdir -p {directory} 2>&1 > /dev/null; sleep 1; rm -r {checkpoint}/cpt.%d 2>&1 > /dev/null; sleep 1; cp -r {checkpoint}/cpt.* {directory}/; sleep 1; {COMMAND}; sleep 1;")


all_tiles = [1024, 2048, 4096, 8192, 16384]
all_tiles_str = ["1K", "2K", "4K", "8K", "16K"]
all_sizes = [200000] #, 2000000]
all_sizes_str = ["200K"] #, "2M"]
all_distances = [256, 1024, 4096, 16384, 65536, 262144, 1048576, 4194304]
all_distances_str = ["256", "1K", "4K", "16K", "64K", "256K", "1M", "4M"]
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

for kernel in all_kernels:
    for size, size_str in zip(all_sizes, all_sizes_str):
        for tile_size, tile_size_str in zip(all_tiles, all_tiles_str):
            for distance, distance_str in zip(all_distances, all_distances_str):
                for mode in ["BASE", "MAA", "CMP"]:
                    directory = f"{DATA_DIR}/checkpoint/{kernel}/M{mode}/D{distance_str}/T{tile_size_str}/S{size_str}/FP32"
                    command = f"{GEM5_DIR}/tests/test-progs/MAA/CISC/test_T{tile_size_str}.o"
                    options = f"{size} {distance} {mode} {kernel}"
                    add_command_checkpoint(directory, command, options)
                    # directory = f"{DATA_DIR}/checkpoint/{kernel}/M{mode}/T{tile_size_str}/S{size_str}/FP64"
                    # command = f"{GEM5_DIR}/tests/test-progs/MAA/CISC/test_double_T{tile_size_str}.o"
                    # add_command_checkpoint(directory, command, options)
    for size, size_str in zip(all_sizes, all_sizes_str):
        for tile_size, tile_size_str in zip(all_tiles, all_tiles_str):
            for distance, distance_str in zip(all_distances, all_distances_str):
                for mode in ["BASE", "MAA", "DMP", "CMP"]:
                    new_mode = mode
                    if mode == "DMP":
                        new_mode = f"BASE"
                    options = f"{size} {distance} {new_mode} {kernel}"

                    checkpoint = f"{DATA_DIR}/checkpoint/{kernel}/M{new_mode}/D{distance_str}/T{tile_size_str}/S{size_str}/FP32"
                    directory = f"{DATA_DIR}/results/{kernel}/M{mode}/D{distance_str}/T{tile_size_str}/S{size_str}/FP32"
                    command = f"{GEM5_DIR}/tests/test-progs/MAA/CISC/test_T{tile_size_str}.o"
                    add_command_run_MAA(directory, checkpoint, command, options, mode, tile_size)
                    
                    # checkpoint = f"{DATA_DIR}/checkpoint/{kernel}/M{new_mode}/T{tile_size_str}/S{size_str}/FP64"
                    # directory = f"{DATA_DIR}/results/M{mode}/T{tile_size_str}/S{size_str}/FP64"
                    # command = f"{GEM5_DIR}/tests/test-progs/MAA/CISC/test_double_T{tile_size_str}.o"
                    # add_command_run_MAA(directory, checkpoint, command, options, mode, tile_size)


if parallelism != 0:
    threads = []
    for i in range(parallelism):
        threads.append(Thread(target=workerthread, args=(i,)))
    
    for i in range(parallelism):
        threads[i].start()
    
    for i in range(parallelism):
        print("T[M]: Waiting for T[{}] to join!".format(i))
        threads[i].join()