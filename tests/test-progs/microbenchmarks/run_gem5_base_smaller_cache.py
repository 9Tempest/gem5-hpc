import argparse
import os
from threading import Thread, Lock

parallelism = 24

tasks = []
lock = Lock()

def workerthread(my_tid):
    global lock
    global tasks
    task = None
    while True:
        with lock:
            if len(tasks) == 0:
                task = None
            else:
                task = tasks.pop(0)
        if task == None:
            print("T[{}]: tasks finished, bye!".format(my_tid))
            break
        else:
            print("T[{}]: executing a new task: {}".format(my_tid, task))
            os.system(task)

# tasks.append(f"./build/ramulator2 -c \"{str(base_config)}\" > {location}/{file}.txt 2>&1")

parser = argparse.ArgumentParser(description='Run gem5.')
parser.add_argument('--cmd', type=str, help='Path to the cmd.', required=True)
parser.add_argument('--options', type=str, help='options to the cmd', default="0")
parser.add_argument('--checkpoint', type=str, help='Path to the checkpoint file.', default=None)
parser.add_argument('--output', type=str, help='Path to the output directory.', required=True)
parser.add_argument('--fast', help='Fast gem5 version.', action=argparse.BooleanOptionalAction)

args = parser.parse_args()

FAST_SIM_TYPE = False if (args.fast == None or args.fast == False) else True
cpu_type = "X86O3CPU"
# cpu_type = "AtomicSimpleCPU"
# core_num = 4
# mem_size = "16GB"
# sys_clock = "3.0GHz"  #DN
# l1d_size = "48kB"
# l1d_assoc = 12
# l1d_hwp_type = "StridePrefetcher"  #DN
# l1d_mshrs = 16
# l1i_size = "32kB"
# l1i_assoc = 8
# l1i_hwp_type = "StridePrefetcher"  #DN
# l1i_mshrs = 16
# l2_size = "1280kB"
# l2_assoc = 20
# l2_hwp_type = "StridePrefetcher"  #DN
# l2_mshrs = 32
# l3_size = "6MB"
# l3_assoc = 48
# l3_mshrs = 256
core_num = 1
mem_size = "16GB"
sys_clock = "3.2GHz"  #DN
l1d_size = "16kB"
l1d_assoc = 8
l1d_hwp_type = "StridePrefetcher"  #DN
l1d_mshrs = 16
l1i_size = "16kB"
l1i_assoc = 8
l1i_hwp_type = "StridePrefetcher"  #DN
l1i_mshrs = 16
l2_size = "128kB"
l2_assoc = 4
l2_hwp_type = "StridePrefetcher"  #DN
l2_mshrs = 32
l3_size = "4MB"
l3_assoc = 16
l3_mshrs = 256
cpu_buffer_enlarge_factor = 1
cpu_register_enlarge_factor = 1
cpu_width_enlarge_factor = 1
mem_type = "Ramulator2"
mem_channels = 1
cmd = args.cmd
options = args.options
program_interval = 1000
checkpoint_address = args.checkpoint
debug_type = None
# debug_type = "LSQ,CacheAll,PseudoInst"
# debug_type = "O3CPUAll,CacheAll,PseudoInst"
out_dir = args.output
if out_dir[-1] == "/":
    out_dir = out_dir[:-1]

def add_command(cpu_buffer_enlarge_factor, cpu_register_enlarge_factor, cpu_width_enlarge_factor):
    m5out_addr = f"{out_dir}/CPU-{cpu_type}/BUFFER-{cpu_buffer_enlarge_factor}/REGISTER-{cpu_register_enlarge_factor}/WIDTH-{cpu_width_enlarge_factor}"
    COMMAND = "time /home/lukezhuz/gem5-hpc/build/X86/gem5."
    if FAST_SIM_TYPE:
        COMMAND += "fast "
    else:
        COMMAND += "opt "
        if debug_type != None:
            COMMAND += f"--debug-flags={debug_type} "
    COMMAND += f"--outdir={m5out_addr} "
    COMMAND += "/home/lukezhuz/gem5-hpc/configs/deprecated/example/se.py "
    COMMAND += f"--cpu-type {cpu_type} "
    COMMAND += f"-n {core_num} "
    COMMAND += f"--cpu-buffer-enlarge-factor={cpu_buffer_enlarge_factor} "
    COMMAND += f"--cpu-register-enlarge-factor={cpu_register_enlarge_factor} "
    COMMAND += f"--cpu-width-enlarge-factor={cpu_width_enlarge_factor} "
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
    COMMAND += f"--l2_mshrs={l2_mshrs} "
    COMMAND += f"--l3cache "
    COMMAND += f"--l3_size={l3_size} "
    COMMAND += f"--l3_assoc={l3_assoc} "
    COMMAND += f"--l3_mshrs={l3_mshrs} "
    COMMAND += "--cacheline_size=64 "
    COMMAND += f"--mem-type {mem_type} "
    COMMAND += f"--mem-channels {mem_channels} "
    COMMAND += f"--cmd {cmd} "
    COMMAND += f"--options \"{options}\" "
    if checkpoint_address != None:
        COMMAND += f"-r 1 "
    COMMAND += f"--prog-interval={program_interval} --work-end-exit-count=1 "
    COMMAND += f"2>&1 "
    COMMAND += "| awk '{ print strftime(), $0; fflush() }' "
    COMMAND += f"| tee {m5out_addr}/logs.txt "
    if checkpoint_address != None:
        tasks.append(f"mkdir -p {m5out_addr} 2>&1 > /dev/null; cp -r {checkpoint_address} {m5out_addr}/; {COMMAND};")
    else:
        tasks.append(f"mkdir -p {m5out_addr} 2>&1 > /dev/null; {COMMAND};")

add_command(1, 1, 1)
# for enlarge_factor in [2, 4, 8, 16]: #, 32, 64, 128]:
#     add_command(enlarge_factor, 1, 1)
#     add_command(enlarge_factor, enlarge_factor, 1)
#     add_command(enlarge_factor, enlarge_factor, enlarge_factor)

if parallelism != 0:
    threads = []
    for i in range(parallelism):
        threads.append(Thread(target=workerthread, args=(i,)))
    
    for i in range(parallelism):
        threads[i].start()
    
    for i in range(parallelism):
        print("T[M]: Waiting for T[{}] to join!".format(i))
        threads[i].join()