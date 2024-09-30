# GEM5-HPC Baseline Configuration

## Added Capabilities

- 3 levels of the cache:
    - `configs/common/CacheConfig.py`
    - `configs/common/Caches.py`
    - `src/mem/XBar.py`

- Ramulator2 backend:
    - `configs/common/MemConfig.py`
    - `ext/ramulator2/`
    - `src/mem/ramulator2.*`
    - `src/mem/SConscript`
    - `configs/ruby/Ruby.py`

- Memory region based stats:
    - `include/gem5/asm/generic/m5ops.h`
    - `include/gem5/m5ops.h`
    - `src/sim/pseudo_inst.*`
    - `util/m5/src/command/`
    - `src/cpu/o3/`
    - `src/mem/packet.*` and `src/mem/request.hh`
    - `src/mem/ramulator2.cc`
    - `src/mem/cache/base.*` and `src/mem/cache/Cache.*`

- Other added/changed files:
    - `configs/common/Options.py`
    - `configs/deprecated/example/se.py`
    - `configs/run_gem5.py`

## How to build?

0. Install Gem5 dependencies from: <https://www.gem5.org/documentation/general_docs/building>
1. Make sure you have access to our Ramulator2 and MAABenchmark repos.
2. Pull our local Gem5: `git clone --recursive git@github.com:MaizeHPC/gem5-hpc.git`
3. Checkout to our repo: `git checkout baseline`
4. Make your own branch **(DO NOT CHANGE THIS BRANCH)**: `git branch [BRANCH_NAME]`
5. Switch to your branch: `git checkout [BRANCH_NAME]`
2. Make Ramulator2:
```bash
cd ext/ramulator2/ramulator2
mkdir build
cd build
cmake ..
make -j
```
3. Build Gem5: `bash make.sh` for the debug version, `bash make_fast.sh` for the fast version (press enter, ‘y’, enter if asked).
4. Add M5 APIs to your test code:
```C++
m5_clear_mem_region(); // resets [Memory Region] <-> [Region ID] mapping
void m5_add_mem_region(void *start, void *end, int8_t id); // adds a [Memory Region] <-> [Region ID] mapping

// Wrap your ROI with this code
m5_work_begin(0, 0);
m5_reset_stats(0, 0);
// ROI CODE
m5_dump_stats(0, 0);
m5_work_end(0, 0);
```
5. Make your test with M5 header and library:
```bash
GEM5_HOME=/home/arkhadem/gem5-hpc
GEM5_INCLUDE="-I${GEM5_HOME}/include/ -I${GEM5_HOME}/util/m5/src/"
GEM5_LIB="-L${GEM5_HOME}/util/m5/build/x86/out"
M5_LIB="$GEM5_HOME/util/m5/build/x86/abi/x86/m5op.S"
g++ $M5_LIB [CPP_FILES] $GEM5_LIB $GEM5_INCLUDE
```
6. Generate the checkpoint:
```bash
OMP_NUM_THREADS=4 ~/gem5-hpc/build/X86/gem5.fast --outdir=[OUTPUT_DIR] ~/gem5-hpc/configs/deprecated/example/se.py --cpu-type AtomicSimpleCPU -n 4 --mem-size '16GB' --cmd [COMMAND] --options [OPTIONS]
```
7. Run Gem5:
```bash
python gem5-hpc/configs/run_gem5.py --cmd [COMMAND] --options [OPTIONS] --checkpoint [CPT_DIR] --output [OUTPUT_DIR] [--fast]
```