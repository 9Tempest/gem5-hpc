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