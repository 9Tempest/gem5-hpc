from common import ObjectList
from common.MAA import *

import m5
from m5.objects import *

from gem5.isas import ISA

def _get_maa_opts(options):
    opts = {}

    if hasattr(options, "maa_num_tiles"):
        opts["num_tiles"] = getattr(options, "maa_num_tiles")

    if hasattr(options, "maa_num_tile_elements"):
        opts["num_tile_elements"] = getattr(options, "maa_num_tile_elements")

    if hasattr(options, "maa_num_regs"):
        opts["num_regs"] = getattr(options, "maa_num_regs")

    if hasattr(options, "maa_num_instructions"):
        opts["num_instructions"] = getattr(options, "maa_num_instructions")
    
    if hasattr(options, "maa_num_stream_access_units"):
        opts["num_stream_access_units"] = getattr(options, "maa_num_stream_access_units")
    
    if hasattr(options, "maa_num_indirect_access_units"):
        opts["num_indirect_access_units"] = getattr(options, "maa_num_indirect_access_units")
    
    if hasattr(options, "maa_num_range_units"):
        opts["num_range_units"] = getattr(options, "maa_num_range_units")
    
    if hasattr(options, "maa_num_alu_units"):
        opts["num_alu_units"] = getattr(options, "maa_num_alu_units")

    if hasattr(options, "maa_num_row_table_rows"):
        opts["num_row_table_rows"] = getattr(options, "maa_num_row_table_rows")
    
    if hasattr(options, "maa_num_row_table_entries_per_row"):
        opts["num_row_table_entries_per_row"] = getattr(options, "maa_num_row_table_entries_per_row")
    
    addr_ranges = []
    start = options.mem_size

    # scratchpad data (cacheable) (4 bytes each)
    SPD_data_size = opts["num_tiles"] * opts["num_tile_elements"] * 4
    addr_ranges.append(AddrRange(start=start, size=SPD_data_size))
    start = addr_ranges[-1].end

    # scratchpad data (noncacheable) (4 bytes each)
    SPD_data_size = opts["num_tiles"] * opts["num_tile_elements"] * 4
    addr_ranges.append(AddrRange(start=start, size=SPD_data_size))
    start = addr_ranges[-1].end

    # scratchpad size (noncacheable) (2 bytes each)
    SPD_size_size = opts["num_tiles"] * 2
    addr_ranges.append(AddrRange(start=start, size=SPD_size_size))
    start = addr_ranges[-1].end

    # scratchpad ready (noncacheable) (2 bytes each)
    SPD_ready_size = opts["num_tiles"] * 2
    addr_ranges.append(AddrRange(start=start, size=SPD_ready_size))
    start = addr_ranges[-1].end

    # scalar registers (noncacheable) (4 bytes each)
    scalar_regs_size = opts["num_regs"] * 4
    addr_ranges.append(AddrRange(start=start, size=scalar_regs_size))
    start = addr_ranges[-1].end

    # instruction file (noncacheable)
    instruction_file_size = 64
    addr_ranges.append(AddrRange(start=start, size=instruction_file_size))
    start = addr_ranges[-1].end

    opts["addr_ranges"] = addr_ranges

    return opts

def get_maa_address(options):
    opts = _get_maa_opts(options)
    start_cacheable_addr = opts["addr_ranges"][0].start
    start_noncacheable_addr = opts["addr_ranges"][1].start
    end_cacheable_addr = Addr(opts["addr_ranges"][0].end)
    end_noncacheable_addr = Addr(opts["addr_ranges"][-1].end)
    size_cacheable_addr = end_cacheable_addr - start_cacheable_addr
    size_noncacheable_addr = end_noncacheable_addr - start_noncacheable_addr
    print(f"MAA Address: cacheable ({start_cacheable_addr}-{end_cacheable_addr} : {size_cacheable_addr}), noncacheable ({start_noncacheable_addr}-{end_noncacheable_addr} : {size_noncacheable_addr})")
    return start_cacheable_addr, size_cacheable_addr, start_noncacheable_addr, size_noncacheable_addr

def config_maa(options, system):
    opts = _get_maa_opts(options)
    system.maa = SharedMAA(clk_domain=system.cpu_clk_domain, **opts)

    # CPU side is derived by the memory side of the memory bus
    system.maa.cpu_side = system.membus.mem_side_ports
    # LLC side derives the cpu side of the L3 bus
    assert(options.l3cache)
    
    # Increasing LLC side packets to accommodate the MAA routing table
    max_routing_table_size = (1 if "num_stream_access_units" not in opts else opts["num_stream_access_units"])
    max_routing_table_size += (1 if "num_indirect_access_units" not in opts else opts["num_indirect_access_units"])
    max_routing_table_size *= (1 if "num_tile_elements" not in opts else opts["num_tile_elements"])
    max_routing_table_size = max(512, max_routing_table_size)
    print(f"MAA max routing table size: {max_routing_table_size}")
    system.maa.max_outstanding_cache_side_packets = max_routing_table_size

    system.tol3bus.max_routing_table_size = max_routing_table_size
    system.maa.cache_side = system.tol3bus.cpu_side_ports
    # Memory side derives the cpu side of the memory bus
    system.maa.mem_side = system.membusnc.cpu_side_ports

    # SPD_data_addr_range = opts["addr_ranges"][0]
    # system.l3.addr_ranges.append(SPD_data_addr_range)
    # print("L3 ranges:")
    # for range in system.l3.addr_ranges:
    #     print(f"start({range.start}), end({range.end})")
    # for i in range(options.num_cpus):
    #     system.cpu[i].dcache.addr_ranges.append(SPD_data_addr_range)
    #     print(f"CPU {i} dcache ranges:")
    #     for range in system.cpu[i].dcache.addr_ranges:
    #         print(f"start({range.start}), end({range.end})")
    #     system.cpu[i].l2cache.addr_ranges.append(SPD_data_addr_range)
    #     print(f"CPU {i} l2cache ranges:")
    #     for range in system.cpu[i].l2cache.addr_ranges:
    #         print(f"start({range.start}), end({range.end})")
    # exit()
