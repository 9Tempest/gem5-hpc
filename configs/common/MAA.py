from m5.defines import buildEnv
from m5.objects import *

from gem5.isas import ISA

class SharedMAA(MAA):
    num_tiles = 32
    num_tile_elements = 1024
    num_regs = 32
    num_instructions = 32
    num_stream_access_units = 1
    num_indirect_access_units = 1
    num_range_units = 1
    num_alu_units = 1
    num_row_table_rows = 64
    num_row_table_entries_per_row = 16
    spd_read_latency = 1
    spd_write_latency = 1
    num_spd_read_ports = 1
    num_spd_write_ports = 1
    rowtable_latency = 1
    ALU_lane_latency = 1
    num_ALU_lanes = 4
    cache_snoop_latency = 1
    max_outstanding_cache_side_packets = 512
    max_outstanding_cpu_side_packets = 512