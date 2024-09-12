from m5.objects.ClockedObject import ClockedObject
from m5.params import *
from m5.proxy import *
from m5.objects.X86MMU import X86MMU
from m5.SimObject import *

class MAA(ClockedObject):
    type = "MAA"
    cxx_header = "mem/MAA/MAA.hh"
    cxx_class = "gem5::MAA"
    cxx_exports = [PyBindMethod("addRamulator")]

    num_tiles = Param.Unsigned(32, "Number of SPD tiles")
    num_tile_elements = Param.Unsigned(1024, "Number of elements in each tile")
    num_regs = Param.Unsigned(32, "Number of 32-bit scalar registers")
    num_instructions = Param.Unsigned(32, "Number of instructions in the instruction file")
    num_stream_access_units = Param.Unsigned(1, "Number of stream access units")
    num_indirect_access_units = Param.Unsigned(1, "Number of indirect access units")
    num_range_units = Param.Unsigned(1, "Number of range units")
    num_alu_units = Param.Unsigned(1, "Number of alu units")
    num_row_table_rows = Param.Unsigned(64, "Number of rows in each row table bank")
    num_row_table_entries_per_row = Param.Unsigned(16, "Number of row table entries (bursts) per row table row")

    cpu_side = ResponsePort("Upstream port closer to the CPU and/or device")
    mem_side = RequestPort("Downstream port closer to DRAM memory")
    cache_side = RequestPort("Downstream port closer to DRAM memory")

    addr_ranges = VectorParam.AddrRange(
        [AllMemory], "Address range for scratchpad data, scratchpad size, scratchpad ready, scalar registers, and instruction file"
    )
    mmu = Param.BaseMMU(X86MMU(), "CPU memory management unit")

    system = Param.System(Parent.any, "System we belong to")

    def addRamulatorInstance(self, simObj):
        self.getCCObject().addRamulator(simObj.getCCObject())