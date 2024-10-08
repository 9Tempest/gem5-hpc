#include "mem/MAA/IndirectAccess.hh"
#include "mem/MAA/MAA.hh"
#include "mem/MAA/SPD.hh"
#include "mem/MAA/IF.hh"
#include "base/trace.hh"
#include "base/types.hh"
#include "debug/MAAIndirect.hh"
#include "mem/packet.hh"
#include "sim/cur_tick.hh"
#include <cassert>
#include <cstdint>
#include <string>

#ifndef TRACING_ON
#define TRACING_ON 1
#endif

namespace gem5 {

///////////////
//
// OFFSET TABLE
//
///////////////
void OffsetTable::allocate(int _my_indirect_id,
                           int _num_tile_elements,
                           IndirectAccessUnit *_indir_access) {
    my_indirect_id = _my_indirect_id;
    num_tile_elements = _num_tile_elements;
    indir_access = _indir_access;
    entries = new OffsetTableEntry[num_tile_elements];
    entries_valid = new bool[num_tile_elements];
    for (int i = 0; i < num_tile_elements; i++) {
        entries_valid[i] = false;
        entries[i].next_itr = -1;
        entries[i].wid = -1;
        entries[i].itr = i;
    }
}
void OffsetTable::insert(int itr, int wid, int last_itr) {
    entries[itr].wid = wid;
    entries[itr].next_itr = -1;
    entries_valid[itr] = true;
    if (last_itr != -1) {
        entries[last_itr].next_itr = itr;
    }
    (*indir_access->maa->stats.IND_NumWordsInserted[my_indirect_id])++;
}
std::vector<OffsetTableEntry> OffsetTable::get_entry_recv(int first_itr) {
    std::vector<OffsetTableEntry> result;
    assert(first_itr != -1);
    int itr = first_itr;
    int prev_itr = itr;
    while (itr != -1) {
        result.push_back(entries[itr]);
        panic_if(entries_valid[itr] == false, "Entry %d is invalid!\n", itr);
        prev_itr = itr;
        itr = entries[itr].next_itr;
        // Invalidate the previous itr
        entries_valid[prev_itr] = false;
        entries[prev_itr].wid = -1;
        entries[prev_itr].next_itr = -1;
    }
    return result;
}
void OffsetTable::check_reset() {
    for (int i = 0; i < num_tile_elements; i++) {
        panic_if(entries_valid[i], "Entry %d is valid: wid(%d) next_itr(%d)!\n",
                 i, entries[i].wid, entries[i].next_itr);
        panic_if(entries[i].next_itr != -1, "Entry %d has next_itr(%d) with wid(%d)!\n",
                 i, entries[i].next_itr, entries[i].wid);
        panic_if(entries[i].wid != -1, "Entry %d has wid(%d) with next_itr(%d)!\n",
                 i, entries[i].wid, entries[i].next_itr);
    }
}
void OffsetTable::reset() {
    for (int i = 0; i < num_tile_elements; i++) {
        entries_valid[i] = false;
        entries[i].next_itr = -1;
        entries[i].wid = -1;
    }
}

///////////////
//
// ROW TABLE ENTRY
//
///////////////
void RowTableEntry::allocate(int _my_indirect_id,
                             int _my_table_id,
                             int _my_table_row_id,
                             int _num_row_table_entries_per_row,
                             OffsetTable *_offset_table,
                             IndirectAccessUnit *_indir_access) {
    my_indirect_id = _my_indirect_id;
    my_table_id = _my_table_id;
    my_table_row_id = _my_table_row_id;
    offset_table = _offset_table;
    indir_access = _indir_access;
    num_row_table_entries_per_row = _num_row_table_entries_per_row;
    entries = new Entry[num_row_table_entries_per_row];
    entries_valid = new bool[num_row_table_entries_per_row];
    last_sent_entry_id = 0;
    for (int i = 0; i < num_row_table_entries_per_row; i++) {
        entries_valid[i] = false;
    }
}
bool RowTableEntry::find_addr(Addr addr) {
    for (int i = 0; i < num_row_table_entries_per_row; i++) {
        if (entries_valid[i] == true && entries[i].addr == addr) {
            return true;
        }
    }
    return false;
}
bool RowTableEntry::insert(Addr addr, int itr, int wid) {
    int free_entry_id = -1;
    for (int i = 0; i < num_row_table_entries_per_row; i++) {
        if (entries_valid[i] == true && entries[i].addr == addr) {
            offset_table->insert(itr, wid, entries[i].last_itr);
            entries[i].last_itr = itr;
            DPRINTF(MAAIndirect, "I[%d] T[%d] R[%d] %s: entry[%d] inserted!\n",
                    my_indirect_id, my_table_id, my_table_row_id, __func__, i);
            return true;
        } else if (entries_valid[i] == false && free_entry_id == -1) {
            free_entry_id = i;
        }
    }
    if (free_entry_id == -1) {
        return false;
    }
    entries[free_entry_id].addr = addr;
    entries[free_entry_id].first_itr = itr;
    entries[free_entry_id].last_itr = itr;
    entries_valid[free_entry_id] = true;
    offset_table->insert(itr, wid, -1);
    DPRINTF(MAAIndirect, "I[%d] T[%d] R[%d] %s: new entry[%d] addr[0x%lx] inserted!\n",
            my_indirect_id, my_table_id, my_table_row_id, __func__, free_entry_id, addr);
    (*indir_access->maa->stats.IND_NumCacheLineInserted[my_indirect_id])++;
    return true;
}
void RowTableEntry::check_reset() {
    for (int i = 0; i < num_row_table_entries_per_row; i++) {
        panic_if(entries_valid[i], "Entry %d is valid: addr(0x%lx)!\n", i, entries[i].addr);
    }
    panic_if(last_sent_entry_id != 0, "Last sent entry id is not 0: %d!\n", last_sent_entry_id);
}
void RowTableEntry::reset() {
    for (int i = 0; i < num_row_table_entries_per_row; i++) {
        entries_valid[i] = false;
    }
    last_sent_entry_id = 0;
}
bool RowTableEntry::get_entry_send(Addr &addr) {
    assert(last_sent_entry_id <= num_row_table_entries_per_row);
    for (; last_sent_entry_id < num_row_table_entries_per_row; last_sent_entry_id++) {
        if (entries_valid[last_sent_entry_id] == true) {
            addr = entries[last_sent_entry_id].addr;
            DPRINTF(MAAIndirect, "I[%d] T[%d] R[%d] %s: sending entry[%d] addr[0x%lx]!\n",
                    my_indirect_id, my_table_id, my_table_row_id, __func__,
                    last_sent_entry_id, addr);
            last_sent_entry_id++;
            return true;
        }
    }
    return false;
}
std::vector<OffsetTableEntry> RowTableEntry::get_entry_recv(Addr addr) {
    for (int i = 0; i < num_row_table_entries_per_row; i++) {
        if (entries_valid[i] == true && entries[i].addr == addr) {
            entries_valid[i] = false;
            DPRINTF(MAAIndirect, "I[%d] T[%d] R[%d] %s: entry[%d] addr[0x%lx] received, setting to invalid!\n",
                    my_indirect_id, my_table_id, my_table_row_id, __func__, i, addr);
            return offset_table->get_entry_recv(entries[i].first_itr);
        }
    }
    return std::vector<OffsetTableEntry>();
}

bool RowTableEntry::all_entries_received() {
    for (int i = 0; i < num_row_table_entries_per_row; i++) {
        if (entries_valid[i] == true) {
            return false;
        }
    }
    last_sent_entry_id = 0;
    return true;
}

///////////////
//
// ROW TABLE
//
///////////////
void RowTable::allocate(int _my_indirect_id,
                        int _my_table_id,
                        int _num_row_table_rows,
                        int _num_row_table_entries_per_row,
                        OffsetTable *_offset_table,
                        IndirectAccessUnit *_indir_access) {
    my_indirect_id = _my_indirect_id;
    my_table_id = _my_table_id;
    offset_table = _offset_table;
    indir_access = _indir_access;
    num_row_table_rows = _num_row_table_rows;
    num_row_table_entries_per_row = _num_row_table_entries_per_row;
    entries = new RowTableEntry[num_row_table_rows];
    entries_valid = new bool[num_row_table_rows];
    last_sent_row_id = 0;
    for (int i = 0; i < num_row_table_rows; i++) {
        entries[i].allocate(my_indirect_id,
                            my_table_id,
                            i,
                            num_row_table_entries_per_row,
                            offset_table,
                            indir_access);
        entries_valid[i] = false;
    }
}
bool RowTable::insert(Addr Grow_addr, Addr addr, int itr, int wid) {
    // 1. Check if the (Row, CL) pair exists
    for (int i = 0; i < num_row_table_rows; i++) {
        if (entries_valid[i] == true && entries[i].Grow_addr == Grow_addr) {
            if (entries[i].find_addr(addr)) {
                DPRINTF(MAAIndirect, "I[%d] T[%d] %s: Grow[0x%lx] addr[0x%lx] found in R[%d]!\n",
                        my_indirect_id, my_table_id, __func__, Grow_addr, addr, i);
                assert(entries[i].insert(addr, itr, wid));
                return true;
            }
        }
    }
    // 2. Check if (Row) exists and can insert the new CL
    // 2.1 At the same time, look for a free row
    int free_row_id = -1;
    for (int i = 0; i < num_row_table_rows; i++) {
        if (entries_valid[i] == true && entries[i].Grow_addr == Grow_addr) {
            if (entries[i].insert(addr, itr, wid)) {
                DPRINTF(MAAIndirect, "I[%d] T[%d] %s: Grow[0x%lx] R[%d] inserted new addr[0x%lx]!\n",
                        my_indirect_id, my_table_id, __func__, Grow_addr, i, addr);
                return true;
            }
        } else if (entries_valid[i] == false && free_row_id == -1) {
            free_row_id = i;
        }
    }
    // 3. Check if we can insert the new Row or we need drain
    if (free_row_id == -1) {
        DPRINTF(MAAIndirect, "I[%d] T[%d] %s: no entry exists or available for Grow[0x%lx] and addr[0x%lx], requires drain!\n",
                my_indirect_id, my_table_id, __func__, Grow_addr, addr);
        return false;
    }
    // 4. Add new (Row), add new (CL)
    DPRINTF(MAAIndirect, "I[%d] T[%d] %s: Grow[0x%lx] adding to new R[%d]!\n",
            my_indirect_id, my_table_id, __func__, Grow_addr, free_row_id);
    entries[free_row_id].Grow_addr = Grow_addr;
    assert(entries[free_row_id].insert(addr, itr, wid) == true);
    entries_valid[free_row_id] = true;
    (*indir_access->maa->stats.IND_NumRowsInserted[my_indirect_id])++;
    return true;
}
float RowTable::getAverageEntriesPerRow() {
    float total_entries = 0.0000;
    for (int i = 0; i < num_row_table_rows; i++) {
        if (entries_valid[i] == true) {
            for (int j = 0; j < num_row_table_entries_per_row; j++) {
                total_entries += entries[i].entries_valid[j] ? 1 : 0;
            }
        }
    }
    return total_entries / num_row_table_rows;
}
void RowTable::check_reset() {
    for (int i = 0; i < num_row_table_rows; i++) {
        entries[i].check_reset();
        panic_if(entries_valid[i], "Row[%d] is valid: Grow_addr(0x%lx)!\n",
                 i, entries[i].Grow_addr);
    }
    panic_if(last_sent_row_id != 0, "Last sent row id is not 0: %d!\n",
             last_sent_row_id);
}
void RowTable::reset() {
    for (int i = 0; i < num_row_table_rows; i++) {
        entries[i].reset();
        entries_valid[i] = false;
    }
    last_sent_row_id = 0;
}
bool RowTable::get_entry_send(Addr &addr) {
    assert(last_sent_row_id <= num_row_table_rows);
    for (; last_sent_row_id < num_row_table_rows; last_sent_row_id++) {
        if (entries_valid[last_sent_row_id] &&
            entries[last_sent_row_id].get_entry_send(addr)) {
            DPRINTF(MAAIndirect, "I[%d] T[%d] %s: row %d retuned!\n", my_indirect_id, my_table_id, __func__, last_sent_row_id);
            return true;
        }
    }
    last_sent_row_id = 0;
    return false;
}
bool RowTable::get_entry_send_first_row(Addr &addr) {
    // This function must be called only when blocked b/c of not entry available
    assert(entries_valid[0]);
    if (entries[0].get_entry_send(addr)) {
        DPRINTF(MAAIndirect, "I[%d] T[%d] %s: retuned addr(0x%lx)!\n",
                my_indirect_id, my_table_id, __func__, addr);
        return true;
    }
    panic_if(last_sent_row_id != 0, "I[%d] T[%d] %s: Last sent row id is not 0: %d!\n", my_indirect_id, my_table_id, __func__, last_sent_row_id);
    return false;
}
std::vector<OffsetTableEntry> RowTable::get_entry_recv(Addr Grow_addr, Addr addr) {
    std::vector<OffsetTableEntry> results;
    for (int i = 0; i < num_row_table_rows; i++) {
        if (entries_valid[i] == true && entries[i].Grow_addr == Grow_addr) {
            std::vector<OffsetTableEntry> result = entries[i].get_entry_recv(addr);
            if (result.size() == 0) {
                DPRINTF(MAAIndirect, "I[%d] T[%d] %s: Grow[0x%lx] addr[0x%lx] hit with R[%d] but no CLs returned!\n",
                        my_indirect_id, my_table_id, __func__, Grow_addr, addr, i);
                continue;
            }
            DPRINTF(MAAIndirect, "I[%d] T[%d] %s: Grow[0x%lx] addr[0x%lx] hit with R[%d], %d entries returned!\n",
                    my_indirect_id, my_table_id, __func__, Grow_addr, addr, i, result.size());
            panic_if(results.size() != 0, "I[%d] T[%d] %s: duplicate entry is not allowed!\n",
                     my_indirect_id, my_table_id, __func__);
            results.insert(results.begin(), result.begin(), result.end());
            if (entries[i].all_entries_received()) {
                DPRINTF(MAAIndirect, "I[%d] T[%d] %s: all R[%d] entries received, setting to invalid!\n",
                        my_indirect_id, my_table_id, __func__, i);
                entries_valid[i] = false;
                entries[i].check_reset();
            }
        }
    }
    return results;
}
///////////////
//
// INDIRECT ACCESS UNIT
//
///////////////
IndirectAccessUnit::IndirectAccessUnit()
    : executeInstructionEvent([this] { executeInstruction(); }, name()),
      sendReadPacketEvent([this] { sendOutstandingReadPacket(); }, name()),
      sendWritePacketEvent([this] { sendOutstandingWritePacket(); }, name()) {
    row_table = nullptr;
    my_instruction = nullptr;
}
void IndirectAccessUnit::allocate(int _my_indirect_id,
                                  int _num_tile_elements,
                                  int _num_row_table_rows,
                                  int _num_row_table_entries_per_row,
                                  Cycles _rowtable_latency,
                                  Cycles _cache_snoop_latency,
                                  MAA *_maa) {
    my_indirect_id = _my_indirect_id;
    maa = _maa;
    num_tile_elements = _num_tile_elements;
    num_row_table_rows = _num_row_table_rows;
    num_row_table_entries_per_row = _num_row_table_entries_per_row;
    num_row_table_banks = maa->m_org[ADDR_CHANNEL_LEVEL] *
                          maa->m_org[ADDR_RANK_LEVEL] * 2;
    rowtable_latency = _rowtable_latency;
    cache_snoop_latency = _cache_snoop_latency;
    offset_table = new OffsetTable();
    offset_table->allocate(my_indirect_id, num_tile_elements, this);
    row_table = new RowTable[num_row_table_banks];
    my_row_table_req_sent = new bool[num_row_table_banks];
    for (int i = 0; i < num_row_table_banks; i++) {
        row_table[i].allocate(my_indirect_id,
                              i,
                              num_row_table_rows,
                              num_row_table_entries_per_row,
                              offset_table,
                              this);
        my_row_table_req_sent[i] = false;
    }
    my_translation_done = false;
    prev_state = Status::max;
    state = Status::Idle;
    my_instruction = nullptr;
    dst_tile_id = -1;
    my_row_table_bank_order.clear();
    for (int bankgroup = 0; bankgroup < 2; bankgroup++) {
        for (int rank = 0; rank < maa->m_org[ADDR_RANK_LEVEL]; rank++) {
            for (int channel = 0; channel < maa->m_org[ADDR_CHANNEL_LEVEL]; channel++) {
                my_row_table_bank_order.push_back(
                    getRowTableIdx(channel, rank, bankgroup));
            }
        }
    }
}
void IndirectAccessUnit::check_reset() {
    for (int i = 0; i < num_row_table_banks; i++) {
        row_table[i].check_reset();
    }
    offset_table->check_reset();
    panic_if(my_outstanding_write_pkts.size() != 0, "Outstanding write packets: %d!\n",
             my_outstanding_write_pkts.size());
    panic_if(my_outstanding_read_pkts.size() != 0, "Outstanding read packets: %d!\n",
             my_outstanding_read_pkts.size());
    panic_if(my_decode_start_tick != 0, "Decode start tick is not 0: %lu!\n", my_decode_start_tick);
    panic_if(my_fill_start_tick != 0, "Fill start tick is not 0: %lu!\n", my_fill_start_tick);
    panic_if(my_drain_start_tick != 0, "Drain start tick is not 0: %lu!\n", my_drain_start_tick);
    panic_if(my_build_start_tick != 0, "Build start tick is not 0: %lu!\n", my_build_start_tick);
    panic_if(my_request_start_tick != 0, "Request start tick is not 0: %lu!\n", my_request_start_tick);
}
Cycles IndirectAccessUnit::updateLatency(int num_spd_read_data_accesses,
                                         int num_spd_read_condidx_accesses,
                                         int num_spd_write_accesses,
                                         int num_rowtable_accesses) {
    if (num_spd_read_data_accesses != 0) {
        // XByte -- 64/X bytes per SPD access
        Cycles get_data_latency = maa->spd->getDataLatency(getCeiling(num_spd_read_data_accesses, my_words_per_cl));
        my_SPD_read_finish_tick = maa->getClockEdge(get_data_latency);
        (*maa->stats.IND_CyclesSPDReadAccess[my_indirect_id]) += get_data_latency;
    }
    if (num_spd_read_condidx_accesses != 0) {
        // 4Byte conditions and indices -- 16 bytes per SPD access
        Cycles get_data_latency = maa->spd->getDataLatency(getCeiling(num_spd_read_condidx_accesses, 16));
        my_SPD_read_finish_tick = maa->getClockEdge(get_data_latency);
        (*maa->stats.IND_CyclesSPDReadAccess[my_indirect_id]) += get_data_latency;
    }
    if (num_spd_write_accesses != 0) {
        // XByte -- 64/X bytes per SPD access
        Cycles set_data_latency = maa->spd->setDataLatency(my_dst_tile, getCeiling(num_spd_write_accesses, my_words_per_cl));
        my_SPD_write_finish_tick = maa->getClockEdge(set_data_latency);
        (*maa->stats.IND_CyclesSPDWriteAccess[my_indirect_id]) += set_data_latency;
    }
    if (num_rowtable_accesses != 0) {
        num_rowtable_accesses = getCeiling(num_rowtable_accesses, num_row_table_banks);
        Cycles access_rowtable_latency = Cycles(num_rowtable_accesses * rowtable_latency);
        if (my_RT_access_finish_tick < curTick())
            my_RT_access_finish_tick = maa->getClockEdge(access_rowtable_latency);
        else
            my_RT_access_finish_tick += maa->getCyclesToTicks(access_rowtable_latency);
        (*maa->stats.IND_CyclesRTAccess[my_indirect_id]) += access_rowtable_latency;
    }
    Tick finish_tick = std::max(std::max(my_SPD_read_finish_tick, my_SPD_write_finish_tick), my_RT_access_finish_tick);
    return maa->getTicksToCycles(finish_tick - curTick());
}
bool IndirectAccessUnit::scheduleNextExecution(bool force) {
    Tick finish_tick = std::max(my_SPD_read_finish_tick, my_SPD_write_finish_tick);
    finish_tick = std::max(finish_tick, my_RT_access_finish_tick);
    if (curTick() < finish_tick) {
        scheduleExecuteInstructionEvent(maa->getTicksToCycles(finish_tick - curTick()));
        return true;
    } else if (force) {
        scheduleExecuteInstructionEvent(Cycles(0));
        return true;
    }
    return false;
}
bool IndirectAccessUnit::scheduleNextSendRead() {
    if (my_outstanding_read_pkts.size() > 0) {
        Cycles latency = Cycles(0);
        if (my_outstanding_read_pkts.begin()->tick > curTick()) {
            latency = maa->getTicksToCycles(my_outstanding_read_pkts.begin()->tick - curTick());
        }
        scheduleSendReadPacketEvent(latency);
        return true;
    }
    return false;
}
bool IndirectAccessUnit::scheduleNextSendWrite() {
    if (my_outstanding_write_pkts.size() > 0) {
        Cycles latency = Cycles(0);
        if (my_outstanding_write_pkts.begin()->tick > curTick()) {
            latency = maa->getTicksToCycles(my_outstanding_write_pkts.begin()->tick - curTick());
        }
        scheduleSendWritePacketEvent(latency);
        return true;
    }
    return false;
}
void IndirectAccessUnit::executeInstruction() {
    switch (state) {
    case Status::Idle: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: idling %s!\n", my_indirect_id, __func__, my_instruction->print());
        prev_state = Status::Idle;
        state = Status::Decode;
        [[fallthrough]];
    }
    case Status::Decode: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: decoding %s!\n", my_indirect_id, __func__, my_instruction->print());

        // Decoding the instruction
        my_idx_tile = my_instruction->src1SpdID;
        my_src_tile = my_instruction->src2SpdID;
        my_dst_tile = my_instruction->dst1SpdID;
        my_cond_tile = my_instruction->condSpdID;
        if (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD) {
            my_word_size = my_instruction->getWordSize(my_dst_tile);
        } else if (my_instruction->opcode == Instruction::OpcodeType::INDIR_ST ||
                   my_instruction->opcode == Instruction::OpcodeType::INDIR_RMW) {
            my_word_size = my_instruction->getWordSize(my_src_tile);
        } else {
            assert(false);
        }
        my_words_per_cl = 64 / my_word_size;
        maa->stats.numInst++;
        (*maa->stats.IND_NumInsts[my_indirect_id])++;
        if (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD) {
            maa->stats.numInst_INDRD++;
        } else if (my_instruction->opcode == Instruction::OpcodeType::INDIR_ST) {
            maa->stats.numInst_INDWR++;
        } else if (my_instruction->opcode == Instruction::OpcodeType::INDIR_RMW) {
            maa->stats.numInst_INDRMW++;
        } else {
            assert(false);
        }
        my_cond_tile_ready = (my_cond_tile == -1) ? true : false;
        my_idx_tile_ready = false;
        my_src_tile_ready = (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD) ? true : false;

        // Initialization
        my_virtual_addr = 0;
        my_base_addr = my_instruction->baseAddr;
        assert(my_outstanding_read_pkts.size() == 0);
        assert(my_outstanding_write_pkts.size() == 0);
        my_received_responses = my_expected_responses = 0;
        offset_table->reset();
        for (int i = 0; i < num_row_table_banks; i++) {
            row_table[i].reset();
            my_row_table_req_sent[i] = false;
        }
        my_i = 0;
        my_max = -1;
        my_SPD_read_finish_tick = curTick();
        my_SPD_write_finish_tick = curTick();
        my_RT_access_finish_tick = curTick();
        my_decode_start_tick = curTick();
        my_fill_start_tick = 0;
        my_drain_start_tick = 0;
        my_build_start_tick = 0;
        my_request_start_tick = 0;

        // Setting the state of the instruction and stream unit
        my_instruction->state = Instruction::Status::Service;
        DPRINTF(MAAIndirect, "I[%d] %s: state set to Fill for request %s!\n", my_indirect_id, __func__, my_instruction->print());
        prev_state = Status::Decode;
        state = Status::Fill;
        [[fallthrough]];
    }
    case Status::Fill: {
        // Reordering the indices
        int num_spd_read_condidx_accesses = 0;
        int num_rowtable_accesses = 0;
        DPRINTF(MAAIndirect, "I[%d] %s: filling %s!\n", my_indirect_id, __func__, my_instruction->print());
        if (scheduleNextExecution()) {
            break;
        }
        // Check if any of the source tiles are ready
        // Set my_max to the size of the ready tile
        if (my_cond_tile != -1) {
            if (maa->spd->getTileStatus(my_cond_tile) == SPD::TileStatus::Finished) {
                my_cond_tile_ready = true;
                if (my_max == -1) {
                    my_max = maa->spd->getSize(my_cond_tile);
                    DPRINTF(MAAIndirect, "I[%d] %s: my_max = cond size (%d)!\n", my_indirect_id, __func__, my_max);
                }
                panic_if(maa->spd->getSize(my_cond_tile) != my_max, "I[%d] %s: cond size (%d) != max (%d)!\n", my_indirect_id, __func__, maa->spd->getSize(my_cond_tile), my_max);
            }
        }
        if (maa->spd->getTileStatus(my_idx_tile) == SPD::TileStatus::Finished) {
            my_idx_tile_ready = true;
            if (my_max == -1) {
                my_max = maa->spd->getSize(my_idx_tile);
                DPRINTF(MAAIndirect, "I[%d] %s: my_max = idx size (%d)!\n", my_indirect_id, __func__, my_max);
            }
            panic_if(maa->spd->getSize(my_idx_tile) != my_max, "I[%d] %s: idx size (%d) != max (%d)!\n", my_indirect_id, __func__, maa->spd->getSize(my_idx_tile), my_max);
        }
        if (my_instruction->opcode != Instruction::OpcodeType::INDIR_LD && maa->spd->getTileStatus(my_src_tile) == SPD::TileStatus::Finished) {
            my_src_tile_ready = true;
        }
        if (my_fill_start_tick == 0) {
            my_fill_start_tick = curTick();
        }
        if (my_request_start_tick != 0) {
            panic_if(prev_state != Status::Request, "I[%d] %s: prev_state(%s) != Request!\n", my_indirect_id, __func__, status_names[(int)prev_state]);
            (*maa->stats.IND_CyclesRequest[my_indirect_id]) += maa->getTicksToCycles(curTick() - my_request_start_tick);
            my_request_start_tick = 0;
        }
        while (true) {
            if (my_max != -1 && my_i >= my_max) {
                if (my_cond_tile_ready == false) {
                    DPRINTF(MAAIndirect, "I[%d] %s: cond tile[%d] not ready, returning!\n", my_indirect_id, __func__, my_cond_tile);
                    // Just a fake access to callback INDIRECT when the condition is ready
                    maa->spd->getElementFinished(my_cond_tile, my_i, 4, (uint8_t)FuncUnitType::INDIRECT, my_indirect_id);
                    return;
                } else if (my_idx_tile_ready == false) {
                    DPRINTF(MAAIndirect, "I[%d] %s: idx tile[%d] not ready, returning!\n", my_indirect_id, __func__, my_idx_tile);
                    // Just a fake access to callback INDIRECT when the idx is ready
                    maa->spd->getElementFinished(my_idx_tile, my_i, 4, (uint8_t)FuncUnitType::INDIRECT, my_indirect_id);
                    return;
                } else if (my_src_tile_ready == false) {
                    DPRINTF(MAAIndirect, "I[%d] %s: src tile[%d] not ready, returning!\n", my_indirect_id, __func__, my_src_tile);
                    // Just a fake access to callback INDIRECT when the src is ready
                    maa->spd->getElementFinished(my_src_tile, my_i, my_word_size, (uint8_t)FuncUnitType::INDIRECT, my_indirect_id);
                    return;
                }
                DPRINTF(MAAIndirect, "I[%d] %s: my_i (%d) >= my_max (%d), finished!\n", my_indirect_id, __func__, my_i, my_max);
                break;
            }
            // if (my_i >= num_tile_elements) {
            //     DPRINTF(MAAIndirect, "I[%d] %s: my_i (%d) >= num_tile_elements (%d), finished!\n", my_indirect_id, __func__, my_i, num_tile_elements);
            //     break;
            // }
            bool cond_ready = my_cond_tile == -1 || maa->spd->getElementFinished(my_cond_tile, my_i, 4, (uint8_t)FuncUnitType::INDIRECT, my_indirect_id);
            bool idx_ready = cond_ready && maa->spd->getElementFinished(my_idx_tile, my_i, 4, (uint8_t)FuncUnitType::INDIRECT, my_indirect_id);
            bool src_ready = idx_ready || (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD ||
                                           maa->spd->getElementFinished(my_src_tile, my_i, my_word_size, (uint8_t)FuncUnitType::INDIRECT, my_indirect_id));
            if (cond_ready == false) {
                DPRINTF(MAAIndirect, "I[%d] %s: cond tile[%d] element[%d] not ready, returning!\n", my_indirect_id, __func__, my_cond_tile, my_i);
            } else if (idx_ready == false) {
                DPRINTF(MAAIndirect, "I[%d] %s: idx tile[%d] element[%d] not ready, returning!\n", my_indirect_id, __func__, my_idx_tile, my_i);
            } else if (src_ready == false) {
                // TODO: this is too early to check src_ready, check it in other stages
                DPRINTF(MAAIndirect, "I[%d] %s: src tile[%d] element[%d] not ready, returning!\n", my_indirect_id, __func__, my_src_tile, my_i);
            }
            if (cond_ready == false || idx_ready == false || src_ready == false) {
                updateLatency(0, num_spd_read_condidx_accesses, 0, num_rowtable_accesses);
                return;
            }
            if (my_cond_tile != -1) {
                num_spd_read_condidx_accesses++;
            }
            if (my_cond_tile == -1 || maa->spd->getData<uint32_t>(my_cond_tile, my_i) != 0) {
                uint32_t idx = maa->spd->getData<uint32_t>(my_idx_tile, my_i);
                num_spd_read_condidx_accesses++;
                Addr vaddr = my_base_addr + my_word_size * idx;
                Addr block_vaddr = addrBlockAlign(vaddr, block_size);
                Addr paddr = translatePacket(block_vaddr);
                Addr block_paddr = addrBlockAlign(paddr, block_size);
                DPRINTF(MAAIndirect, "I[%d] %s: idx = %u, addr = 0x%lx!\n",
                        my_indirect_id, __func__, idx, block_paddr);
                uint16_t wid = (vaddr - block_vaddr) / my_word_size;
                std::vector<int> addr_vec = maa->map_addr(block_paddr);
                Addr Grow_addr = maa->calc_Grow_addr(addr_vec);
                my_row_table_idx = getRowTableIdx(addr_vec[ADDR_CHANNEL_LEVEL], addr_vec[ADDR_RANK_LEVEL], addr_vec[ADDR_BANKGROUP_LEVEL]);
                assert(my_row_table_idx < num_row_table_banks);
                DPRINTF(MAAIndirect, "I[%d] %s: inserting vaddr(0x%lx), paddr(0x%lx), MAP(RO: %d, BA: %d, BG: %d, RA: %d, CO: %d, CH: %d), Grow(0x%lx), itr(%d), idx(%d), wid(%d) to T[%d]\n",
                        my_indirect_id, __func__,
                        block_vaddr, block_paddr,
                        addr_vec[ADDR_ROW_LEVEL], addr_vec[ADDR_BANK_LEVEL], addr_vec[ADDR_BANKGROUP_LEVEL], addr_vec[ADDR_RANK_LEVEL], addr_vec[ADDR_COLUMN_LEVEL], addr_vec[ADDR_CHANNEL_LEVEL],
                        Grow_addr, my_i, idx, wid, my_row_table_idx);
                bool inserted = row_table[my_row_table_idx].insert(Grow_addr, block_paddr, my_i, wid);
                num_rowtable_accesses++;
                if (inserted == false) {
                    updateLatency(0, num_spd_read_condidx_accesses, 0, num_rowtable_accesses);
                    scheduleNextExecution(true);
                    prev_state = Status::Fill;
                    state = Status::Drain;
                    (*maa->stats.IND_NumDrains[my_indirect_id])++;
                    return;
                }
            } else if (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD) {
                DPRINTF(MAAIndirect, "I[%d] %s: SPD[%d][%d] = %u (cond not taken)\n", my_indirect_id, __func__, my_dst_tile, my_i, 0);
                switch (my_instruction->datatype) {
                case Instruction::DataType::UINT32_TYPE: {
                    maa->spd->setData<uint32_t>(my_dst_tile, my_i, 0);
                    break;
                }
                case Instruction::DataType::INT32_TYPE: {
                    maa->spd->setData<int32_t>(my_dst_tile, my_i, 0);
                    break;
                }
                case Instruction::DataType::FLOAT32_TYPE: {
                    maa->spd->setData<float>(my_dst_tile, my_i, 0);
                    break;
                }
                case Instruction::DataType::UINT64_TYPE: {
                    maa->spd->setData<uint64_t>(my_dst_tile, my_i, 0);
                    break;
                }
                case Instruction::DataType::INT64_TYPE: {
                    maa->spd->setData<int64_t>(my_dst_tile, my_i, 0);
                    break;
                }
                case Instruction::DataType::FLOAT64_TYPE: {
                    maa->spd->setData<double>(my_dst_tile, my_i, 0);
                    break;
                }
                default:
                    assert(false);
                }
            }
            my_i++;
        }
        if (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD) {
            panic_if(my_max != -1 && my_i != my_max, "I[%d] %s: my_i(%d) != my_max(%d)!\n", my_indirect_id, __func__, my_i, my_max);
            maa->spd->setSize(my_dst_tile, my_i);
        }
        DPRINTF(MAAIndirect, "I[%d] %s: state set to Build for %s!\n",
                my_indirect_id, __func__, my_instruction->print());

        updateLatency(0, num_spd_read_condidx_accesses, 0, num_rowtable_accesses);
        scheduleNextExecution(true);
        prev_state = Status::Fill;
        state = Status::Build;
        return;
    }
    case Status::Drain: {
        DPRINTF(MAAIndirect, "I[%d] %s: Draining %s!\n", my_indirect_id, __func__, my_instruction->print());
        if (scheduleNextExecution()) {
            break;
        }
        if (my_drain_start_tick == 0) {
            my_drain_start_tick = curTick();
        }
        if (my_fill_start_tick != 0) {
            panic_if(prev_state != Status::Fill, "I[%d] %s: prev_state(%s) != Fill!\n", my_indirect_id, __func__, status_names[(int)prev_state]);
            (*maa->stats.IND_CyclesFill[my_indirect_id]) += maa->getTicksToCycles(curTick() - my_fill_start_tick);
            my_fill_start_tick = 0;
        }
        int num_rowtable_accesses = 0;
        Addr addr;
        while (row_table[my_row_table_idx].get_entry_send_first_row(addr)) {
            DPRINTF(MAAIndirect, "I[%d] %s: Creating packet for bank[%d], addr[0x%lx]!\n",
                    my_indirect_id, __func__, my_row_table_idx, addr);
            my_expected_responses++;
            num_rowtable_accesses++;
            createReadPacket(addr, num_rowtable_accesses * rowtable_latency, false, true);
        }
        DPRINTF(MAAIndirect, "I[%d] %s: drain completed, going to the request state...\n", my_indirect_id, __func__);
        updateLatency(0, 0, 0, num_rowtable_accesses);
        prev_state = Status::Drain;
        state = Status::Request;
        scheduleNextSendRead();
        break;
    }
    case Status::Build: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: Building %s requests!\n", my_indirect_id, __func__, my_instruction->print());
        if (scheduleNextExecution()) {
            break;
        }
        if (my_build_start_tick == 0) {
            my_build_start_tick = curTick();
        }
        if (my_fill_start_tick != 0) {
            panic_if(prev_state != Status::Fill, "I[%d] %s: prev_state(%s) != Fill!\n", my_indirect_id, __func__, status_names[(int)prev_state]);
            (*maa->stats.IND_CyclesFill[my_indirect_id]) += maa->getTicksToCycles(curTick() - my_fill_start_tick);
            my_fill_start_tick = 0;
        }
        int last_row_table_sent = 0;
        int num_rowtable_accesses = 0;
        Addr addr;
        while (true) {
            if (checkAllRowTablesSent()) {
                break;
            }
            for (; last_row_table_sent < num_row_table_banks; last_row_table_sent++) {
                int row_table_idx = my_row_table_bank_order[last_row_table_sent];
                assert(row_table_idx < num_row_table_banks);
                DPRINTF(MAAIndirect, "I[%d] %s: Checking row table bank[%d]!\n", my_indirect_id, __func__, row_table_idx);
                if (my_row_table_req_sent[row_table_idx] == false) {
                    if (row_table[row_table_idx].get_entry_send(addr)) {
                        DPRINTF(MAAIndirect, "I[%d] %s: Creating packet for bank[%d], addr[0x%lx]!\n",
                                my_indirect_id, __func__, row_table_idx, addr);
                        my_expected_responses++;
                        num_rowtable_accesses++;
                        createReadPacket(addr, num_rowtable_accesses * rowtable_latency, false, true);
                    } else {
                        DPRINTF(MAAIndirect, "I[%d] %s: T[%d] has nothing, setting sent to true!\n", my_indirect_id, __func__, row_table_idx);
                        my_row_table_req_sent[row_table_idx] = true;
                    }
                } else {
                    DPRINTF(MAAIndirect, "I[%d] %s: T[%d] has already sent the requests!\n", my_indirect_id, __func__, row_table_idx);
                }
            }
            last_row_table_sent = (last_row_table_sent >= num_row_table_banks) ? 0 : last_row_table_sent;
        }
        DPRINTF(MAAIndirect, "I[%d] %s: state set to Request for %s!\n", my_indirect_id, __func__, my_instruction->print());
        updateLatency(0, 0, 0, num_rowtable_accesses);
        prev_state = Status::Build;
        state = Status::Request;
        if (my_received_responses == my_expected_responses) {
            panic_if(my_outstanding_read_pkts.empty() == false, "I[%d] %s: %d outstanding read packets remaining!\n",
                     my_outstanding_read_pkts.size(), my_indirect_id, __func__);
            panic_if(my_outstanding_write_pkts.empty() == false, "I[%d] %s: %d outstanding write packets remaining!\n",
                     my_outstanding_write_pkts.size(), my_indirect_id, __func__);
            DPRINTF(MAAIndirect, "I[%d] %s: all responses received, calling execution again in state %s!\n",
                    my_indirect_id, __func__, status_names[(int)state]);
            scheduleNextExecution(true);
        } else {
            scheduleNextSendRead();
        }
        break;
    }
    case Status::Request: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: requesting %s!\n", my_indirect_id, __func__, my_instruction->print());
        if (scheduleNextExecution()) {
            break;
        }
        if (my_request_start_tick == 0) {
            my_request_start_tick = curTick();
        }
        if (my_drain_start_tick != 0) {
            panic_if(prev_state != Status::Drain, "I[%d] %s: prev_state(%s) != Drain!\n", my_indirect_id, __func__, status_names[(int)prev_state]);
            (*maa->stats.IND_CyclesDrain[my_indirect_id]) += maa->getTicksToCycles(curTick() - my_drain_start_tick);
            my_drain_start_tick = 0;
        }
        if (my_build_start_tick != 0) {
            panic_if(prev_state != Status::Build, "I[%d] %s: prev_state(%s) != Build!\n", my_indirect_id, __func__, status_names[(int)prev_state]);
            (*maa->stats.IND_CyclesBuild[my_indirect_id]) += maa->getTicksToCycles(curTick() - my_build_start_tick);
            my_build_start_tick = 0;
        }
        panic_if(my_received_responses != my_expected_responses, "I[%d] %s: received_responses(%d) != sent_requests(%d)!\n",
                 my_indirect_id, __func__, my_received_responses, my_expected_responses);
        if (prev_state == Status::Drain) {
            DPRINTF(MAAIndirect, "I[%d] %s: request completed, returning to the fill stage...\n", my_indirect_id, __func__);
            prev_state = Status::Request;
            state = Status::Fill;
        } else {
            assert(prev_state == Status::Build);
            DPRINTF(MAAIndirect, "I[%d] %s: request completed, state set to respond!\n", my_indirect_id, __func__);
            prev_state = Status::Request;
            state = Status::Response;
        }
        scheduleNextExecution(true);
        break;
    }
    case Status::Response: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: responding %s!\n", my_indirect_id, __func__, my_instruction->print());
        panic_if(scheduleNextExecution(), "I[%d] %s: Execution is not completed!\n", my_indirect_id, __func__);
        panic_if(scheduleNextSendRead(), "I[%d] %s: Sending reads is not completed!\n", my_indirect_id, __func__);
        panic_if(scheduleNextSendWrite(), "I[%d] %s: Sending writes is not completed!\n", my_indirect_id, __func__);
        panic_if(my_cond_tile_ready == false, "I[%d] %s: cond tile[%d] is not ready!\n", my_indirect_id, __func__, my_cond_tile);
        panic_if(my_idx_tile_ready == false, "I[%d] %s: idx tile[%d] is not ready!\n", my_indirect_id, __func__, my_idx_tile);
        panic_if(my_src_tile_ready == false, "I[%d] %s: src tile[%d] is not ready!\n", my_indirect_id, __func__, my_src_tile);
        DPRINTF(MAAIndirect, "I[%d] %s: state set to finish for request %s!\n", my_indirect_id, __func__, my_instruction->print());
        my_instruction->state = Instruction::Status::Finish;
        if (my_request_start_tick != 0) {
            panic_if(prev_state != Status::Request, "I[%d] %s: prev_state(%s) != Request!\n", my_indirect_id, __func__, status_names[(int)prev_state]);
            (*maa->stats.IND_CyclesRequest[my_indirect_id]) += maa->getTicksToCycles(curTick() - my_request_start_tick);
            my_request_start_tick = 0;
        }
        Cycles total_cycles = maa->getTicksToCycles(curTick() - my_decode_start_tick);
        maa->stats.cycles += total_cycles;
        my_decode_start_tick = 0;
        prev_state = Status::max;
        state = Status::Idle;
        check_reset();
        maa->finishInstructionCompute(my_instruction);
        if (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD) {
            maa->stats.cycles_INDRD += total_cycles;
        } else if (my_instruction->opcode == Instruction::OpcodeType::INDIR_ST) {
            maa->stats.cycles_INDWR += total_cycles;
        } else {
            maa->stats.cycles_INDRMW += total_cycles;
        }
        my_instruction = nullptr;
        break;
    }
    default:
        assert(false);
    }
}
bool IndirectAccessUnit::checkAllRowTablesSent() {
    for (int i = 0; i < num_row_table_banks; i++) {
        if (my_row_table_req_sent[i] == false) {
            return false;
        }
    }
    return true;
}
int IndirectAccessUnit::getRowTableIdx(int channel, int rank, int bankgroup) {
    return (channel * maa->m_org[ADDR_RANK_LEVEL] * 2) +
           (rank * 2) +
           (bankgroup % 2);
}
void IndirectAccessUnit::createReadPacket(Addr addr, int latency, bool is_cached, bool is_snoop) {
    /**** Packet generation ****/
    RequestPtr real_req = std::make_shared<Request>(addr, block_size, flags, maa->requestorId);
    PacketPtr read_pkt;
    if (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD) {
        read_pkt = new Packet(real_req, MemCmd::ReadSharedReq);
    } else {
        read_pkt = new Packet(real_req, MemCmd::ReadExReq);
    }
    read_pkt->allocate();
    if (is_snoop) {
        read_pkt->setExpressSnoop();
        read_pkt->headerDelay = read_pkt->payloadDelay = 0;
    }
    IndirectAccessUnit::IndirectPacket new_packet = IndirectAccessUnit::IndirectPacket(read_pkt, is_cached, is_snoop, maa->getClockEdge(Cycles(latency)));
    my_outstanding_read_pkts.insert(new_packet);
    DPRINTF(MAAIndirect, "I[%d] %s: created %s to send in %d cycles at tick %u, is cached %s\n", my_indirect_id, __func__, read_pkt->print(), latency, new_packet.tick, is_cached ? "True" : "False");
}
void IndirectAccessUnit::createReadPacketEvict(Addr addr) {
    /**** Packet generation ****/
    RequestPtr real_req = std::make_shared<Request>(addr, block_size, flags, maa->requestorId);
    PacketPtr my_pkt = new Packet(real_req, MemCmd::CleanEvict);
    IndirectAccessUnit::IndirectPacket new_packet = IndirectAccessUnit::IndirectPacket(my_pkt, true, false, maa->getClockEdge(Cycles(0)));
    my_outstanding_read_pkts.insert(new_packet);
    DPRINTF(MAAIndirect, "I[%d] %s: created %s to send in %d cycles\n", my_indirect_id, __func__, my_pkt->print(), 0);
}
bool IndirectAccessUnit::sendOutstandingReadPacket() {
    DPRINTF(MAAIndirect, "I[%d] %s: sending %d outstanding read packets...\n", my_indirect_id, __func__, my_outstanding_read_pkts.size());
    bool packet_sent = false;
    while (my_outstanding_read_pkts.empty() == false) {
        IndirectAccessUnit::IndirectPacket read_pkt = *my_outstanding_read_pkts.begin();
        DPRINTF(MAAIndirect, "I[%d] %s: trying sending %s to %s as a %s at time %u from %d packets\n",
                my_indirect_id, __func__, read_pkt.packet->print(),
                read_pkt.is_snoop ? "cpuSide" : read_pkt.is_cached ? "cacheSide"
                                                                   : "memSide",
                read_pkt.is_snoop ? "snoop" : "request", read_pkt.tick, my_outstanding_read_pkts.size());
        if (read_pkt.tick > curTick()) {
            DPRINTF(MAAIndirect, "I[%d] %s: waiting for %d cycles\n",
                    my_indirect_id, __func__, maa->getTicksToCycles(read_pkt.tick - curTick()));
            scheduleNextSendRead();
            return false;
        }
        if (read_pkt.is_snoop) {
            DPRINTF(MAAIndirect, "I[%d] %s: trying sending snoop %s to cpuSide\n", my_indirect_id, __func__, read_pkt.packet->print());
            if (maa->cpuSidePort.sendSnoopPacket((uint8_t)FuncUnitType::INDIRECT, my_indirect_id, read_pkt.packet) == false) {
                DPRINTF(MAAIndirect, "I[%d] %s: send failed, leaving send packet...\n", my_indirect_id, __func__);
                return false;
            }
            DPRINTF(MAAIndirect, "I[%d] %s: successfully sent as a snoop to cpuSide, cache responding: %s, has sharers %s, had writable %s, satisfied %s, is block cached %s...\n",
                    my_indirect_id, __func__, read_pkt.packet->cacheResponding(), read_pkt.packet->hasSharers(), read_pkt.packet->responderHadWritable(), read_pkt.packet->satisfied(), read_pkt.packet->isBlockCached());
            packet_sent = true;
            my_outstanding_read_pkts.erase(my_outstanding_read_pkts.begin());
            if (read_pkt.packet->cacheResponding() == true) {
                DPRINTF(MAAIndirect, "I[%d] %s: a cache in the O/M state will respond, send successfull...\n", my_indirect_id, __func__);
                (*maa->stats.IND_LoadsCacheHitResponding[my_indirect_id])++;
            } else if (read_pkt.packet->hasSharers() == true) {
                DPRINTF(MAAIndirect, "I[%d] %s: There's a cache in the E/S state will respond, creating again and sending to cache\n", my_indirect_id, __func__);
                panic_if(read_pkt.packet->needsWritable(), "I[%d] %s: packet needs writable!\n", my_indirect_id, __func__);
                createReadPacket(read_pkt.packet->getAddr(), 0, true, false);
                (*maa->stats.IND_LoadsCacheHitAccessing[my_indirect_id])++;
            } else {
                DPRINTF(MAAIndirect, "I[%d] %s: no cache responds (I), creating again and sending to memory\n", my_indirect_id, __func__);
                createReadPacket(read_pkt.packet->getAddr(), 0, false, false);
                (*maa->stats.IND_LoadsMemAccessing[my_indirect_id])++;
            }
            // We can continue sending the next packets anyway
        } else if (read_pkt.is_cached) {
            DPRINTF(MAAIndirect, "I[%d] %s: trying sending %s to cacheSide\n", my_indirect_id, __func__, read_pkt.packet->print());
            if (maa->cacheSidePort.sendPacket((uint8_t)FuncUnitType::INDIRECT, my_indirect_id, read_pkt.packet) == false) {
                DPRINTF(MAAIndirect, "I[%d] %s: send failed, leaving send packet...\n", my_indirect_id, __func__);
                // A send has failed, we cannot continue sending the next packets
                return false;
            } else {
                packet_sent = true;
                my_outstanding_read_pkts.erase(my_outstanding_read_pkts.begin());
                // A packet is sent successfully, we can continue sending the next packets
            }
        } else {
            DPRINTF(MAAIndirect, "I[%d] %s: trying sending %s to memSide\n", my_indirect_id, __func__, read_pkt.packet->print());
            if (maa->memSidePort.sendPacket(my_indirect_id, read_pkt.packet) == false) {
                DPRINTF(MAAIndirect, "I[%d] %s: send failed, leaving send packet...\n", my_indirect_id, __func__);
                // A send has failed, we cannot continue sending the next packets
                return false;
            } else {
                my_outstanding_read_pkts.erase(my_outstanding_read_pkts.begin());
                // A packet is sent successfully, we can continue sending the next packets
            }
        }
    }
    if (packet_sent && (my_received_responses == my_expected_responses)) {
        DPRINTF(MAAIndirect, "I[%d] %s: all responses received, calling execution again in state %s!\n",
                my_indirect_id, __func__, status_names[(int)state]);
        scheduleNextExecution(true);
    } else {
        DPRINTF(MAAIndirect, "I[%d] %s: expected: %d, received: %d, packet send: %d!\n", my_indirect_id, __func__, my_expected_responses, my_received_responses, packet_sent);
    }
    return true;
}
bool IndirectAccessUnit::sendOutstandingWritePacket() {
    bool packet_sent = false;
    while (my_outstanding_write_pkts.empty() == false) {
        IndirectAccessUnit::IndirectPacket write_pkt = *my_outstanding_write_pkts.begin();
        if (write_pkt.tick > curTick()) {
            DPRINTF(MAAIndirect, "I[%d] %s: waiting for %d cycles to send %s to memory\n",
                    my_indirect_id, __func__, maa->getTicksToCycles(write_pkt.tick - curTick()), write_pkt.packet->print());
            scheduleNextSendWrite();
            return false;
        }
        panic_if(write_pkt.is_cached || write_pkt.is_snoop, "I[%d] %s: write packet is not for memory!\n", my_indirect_id, __func__);
        DPRINTF(MAAIndirect, "I[%d] %s: trying sending %s to memory\n", my_indirect_id, __func__, write_pkt.packet->print());
        if (maa->memSidePort.sendPacket(my_indirect_id,
                                        write_pkt.packet) == false) {
            DPRINTF(MAAIndirect, "I[%d] %s: send failed, leaving execution...\n", my_indirect_id, __func__);
            return false;
        } else {
            my_outstanding_write_pkts.erase(my_outstanding_write_pkts.begin());
            my_received_responses++;
            packet_sent = true;
        }
    }
    if (packet_sent && (my_received_responses == my_expected_responses)) {
        DPRINTF(MAAIndirect, "I[%d] %s: all responses received, calling execution again in state %s!\n",
                my_indirect_id, __func__, status_names[(int)state]);
        scheduleNextExecution(true);
    } else {
        DPRINTF(MAAIndirect, "I[%d] %s: expected: %d, received: %d, packet send: %d!\n", my_indirect_id, __func__, my_expected_responses, my_received_responses, packet_sent);
    }
    return true;
}
bool IndirectAccessUnit::recvData(const Addr addr,
                                  uint8_t *dataptr,
                                  bool is_block_cached) {
    std::vector addr_vec = maa->map_addr(addr);
    Addr Grow_addr = maa->calc_Grow_addr(addr_vec);
    int row_table_idx = getRowTableIdx(addr_vec[ADDR_CHANNEL_LEVEL],
                                       addr_vec[ADDR_RANK_LEVEL],
                                       addr_vec[ADDR_BANKGROUP_LEVEL]);
    std::vector<OffsetTableEntry> entries = row_table[row_table_idx].get_entry_recv(Grow_addr, addr);
    DPRINTF(MAAIndirect, "I[%d] %s: %d entries received for addr(0x%lx), Grow(x%lx) from T[%d]!\n",
            my_indirect_id, __func__, entries.size(),
            addr, Grow_addr, row_table_idx);
    if (entries.size() == 0) {
        return false;
    }
    uint8_t new_data[block_size];
    uint32_t *dataptr_u32_typed = (uint32_t *)dataptr;
    uint64_t *dataptr_u64_typed = (uint64_t *)dataptr;
    std::memcpy(new_data, dataptr, block_size);
    int num_recv_spd_read_accesses = 0;
    int num_recv_spd_write_accesses = 0;
    int num_recv_rt_accesses = entries.size();
    for (auto entry : entries) {
        int itr = entry.itr;
        int wid = entry.wid;
        DPRINTF(MAAIndirect, "I[%d] %s: itr (%d) wid (%d) matched!\n", my_indirect_id, __func__, itr, wid);
        switch (my_instruction->opcode) {
        case Instruction::OpcodeType::INDIR_LD: {
            if (my_word_size == 4) {
                maa->spd->setData<uint32_t>(my_dst_tile, itr, dataptr_u32_typed[wid]);
            } else {
                maa->spd->setData<uint64_t>(my_dst_tile, itr, dataptr_u64_typed[wid]);
            }
            num_recv_spd_write_accesses++;
            break;
        }
        case Instruction::OpcodeType::INDIR_ST: {
            if (my_word_size == 4) {
                ((uint32_t *)new_data)[wid] = maa->spd->getData<uint32_t>(my_src_tile, itr);
                DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] = SPD[%d][%d] = %f!\n",
                        my_indirect_id, __func__, wid, my_src_tile, itr, ((float *)new_data)[wid]);
            } else {
                ((uint64_t *)new_data)[wid] = maa->spd->getData<uint64_t>(my_src_tile, itr);
                DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] = SPD[%d][%d] = %f!\n",
                        my_indirect_id, __func__, wid, my_src_tile, itr, ((double *)new_data)[wid]);
            }
            num_recv_spd_read_accesses++;
            break;
        }
        case Instruction::OpcodeType::INDIR_RMW: {
            num_recv_spd_read_accesses++;
            switch (my_instruction->datatype) {
            case Instruction::DataType::UINT32_TYPE: {
                uint32_t word_data = maa->spd->getData<uint32_t>(my_src_tile, itr);
                assert(my_instruction->optype == Instruction::OPType::ADD_OP);
                DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] (%u) += SPD[%d][%d] (%u) = %u!\n",
                        my_indirect_id, __func__, wid, ((uint32_t *)new_data)[wid], my_src_tile, itr, word_data, ((uint32_t *)new_data)[wid] + word_data);
                ((uint32_t *)new_data)[wid] += word_data;
                break;
            }
            case Instruction::DataType::INT32_TYPE: {
                int32_t word_data = maa->spd->getData<int32_t>(my_src_tile, itr);
                assert(my_instruction->optype == Instruction::OPType::ADD_OP);
                DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] (%d) += SPD[%d][%d] (%d) = %d!\n",
                        my_indirect_id, __func__, wid, ((int32_t *)new_data)[wid], my_src_tile, itr, word_data, ((int32_t *)new_data)[wid] + word_data);
                ((int32_t *)new_data)[wid] += word_data;
                break;
            }
            case Instruction::DataType::FLOAT32_TYPE: {
                float word_data = maa->spd->getData<float>(my_src_tile, itr);
                assert(my_instruction->optype == Instruction::OPType::ADD_OP);
                DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] (%f) += SPD[%d][%d] (%f) = %f!\n",
                        my_indirect_id, __func__, wid, ((float *)new_data)[wid], my_src_tile, itr, word_data, ((float *)new_data)[wid] + word_data);
                ((float *)new_data)[wid] += word_data;
                break;
            }
            case Instruction::DataType::UINT64_TYPE: {
                uint64_t word_data = maa->spd->getData<uint64_t>(my_src_tile, itr);
                assert(my_instruction->optype == Instruction::OPType::ADD_OP);
                DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] (%lu) += SPD[%d][%d] (%lu) = %lu!\n",
                        my_indirect_id, __func__, wid, ((uint64_t *)new_data)[wid], my_src_tile, itr, word_data, ((uint64_t *)new_data)[wid] + word_data);
                ((uint64_t *)new_data)[wid] += word_data;
                break;
            }
            case Instruction::DataType::INT64_TYPE: {
                int64_t word_data = maa->spd->getData<int64_t>(my_src_tile, itr);
                assert(my_instruction->optype == Instruction::OPType::ADD_OP);
                DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] (%ld) += SPD[%d][%d] (%ld) = %ld!\n",
                        my_indirect_id, __func__, wid, ((int64_t *)new_data)[wid], my_src_tile, itr, word_data, ((int64_t *)new_data)[wid] + word_data);
                ((int64_t *)new_data)[wid] += word_data;
                break;
            }
            case Instruction::DataType::FLOAT64_TYPE: {
                double word_data = maa->spd->getData<double>(my_src_tile, itr);
                assert(my_instruction->optype == Instruction::OPType::ADD_OP);
                DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] (%lf) += SPD[%d][%d] (%lf) = %lf!\n",
                        my_indirect_id, __func__, wid, ((double *)new_data)[wid], my_src_tile, itr, word_data, ((double *)new_data)[wid] + word_data);
                ((double *)new_data)[wid] += word_data;
                break;
            }
            default:
                assert(false);
            }
            break;
        }
        default:
            assert(false);
        }
    }

    Cycles total_latency = updateLatency(num_recv_spd_read_accesses, 0, num_recv_spd_write_accesses, num_recv_rt_accesses);
    if (my_instruction->opcode == Instruction::OpcodeType::INDIR_ST || my_instruction->opcode == Instruction::OpcodeType::INDIR_RMW) {
        RequestPtr real_req = std::make_shared<Request>(addr, block_size, flags, maa->requestorId);
        PacketPtr write_pkt = new Packet(real_req, MemCmd::WritebackDirty);
        write_pkt->allocate();
        write_pkt->setData(new_data);
        for (int i = 0; i < block_size / my_word_size; i++) {
            if (my_word_size == 4)
                DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] = %f!\n",
                        my_indirect_id, __func__, i, write_pkt->getPtr<float>()[i]);
            else
                DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] = %f!\n",
                        my_indirect_id, __func__, i, write_pkt->getPtr<double>()[i]);
        }
        DPRINTF(MAAIndirect, "I[%d] %s: created %s to send in %d cycles\n", my_indirect_id, __func__, write_pkt->print(), total_latency);
        my_outstanding_write_pkts.insert(IndirectAccessUnit::IndirectPacket(write_pkt, false, false, maa->getClockEdge(total_latency)));
        (*maa->stats.IND_StoresMemAccessing[my_indirect_id])++;
        scheduleNextSendWrite();
    } else {
        my_received_responses++;
        if (is_block_cached) {
            createReadPacketEvict(addr);
            (*maa->stats.IND_Evicts[my_indirect_id])++;
            scheduleNextSendRead();
        } else {
            if (my_received_responses == my_expected_responses) {
                DPRINTF(MAAIndirect, "I[%d] %s: all responses received, calling execution again!\n", my_indirect_id, __func__);
                scheduleNextExecution(true);
            } else {
                DPRINTF(MAAIndirect, "I[%d] %s: expected: %d, received: %d responses!\n", my_indirect_id, __func__, my_expected_responses, my_received_responses);
            }
        }
    }
    return true;
}
Addr IndirectAccessUnit::translatePacket(Addr vaddr) {
    /**** Address translation ****/
    RequestPtr translation_req = std::make_shared<Request>(vaddr,
                                                           block_size,
                                                           flags, maa->requestorId,
                                                           my_instruction->PC,
                                                           my_instruction->CID);
    ThreadContext *tc = maa->system->threads[my_instruction->CID];
    maa->mmu->translateTiming(translation_req, tc, this, BaseMMU::Read);
    // The above function immediately does the translation and calls the finish function
    assert(my_translation_done);
    my_translation_done = false;
    return my_translated_addr;
}
void IndirectAccessUnit::finish(const Fault &fault,
                                const RequestPtr &req, ThreadContext *tc, BaseMMU::Mode mode) {
    assert(fault == NoFault);
    assert(my_translation_done == false);
    my_translation_done = true;
    my_translated_addr = req->getPaddr();
}
void IndirectAccessUnit::setInstruction(Instruction *_instruction) {
    assert(my_instruction == nullptr);
    my_instruction = _instruction;
}
void IndirectAccessUnit::scheduleExecuteInstructionEvent(int latency) {
    DPRINTF(MAAIndirect, "I[%d] %s: scheduling execute for the IndirectAccess Unit in the next %d cycles!\n", my_indirect_id, __func__, latency);
    panic_if(latency < 0, "Negative latency of %d!\n", latency);
    Tick new_when = maa->getClockEdge(Cycles(latency));
    if (!executeInstructionEvent.scheduled()) {
        maa->schedule(executeInstructionEvent, new_when);
    } else {
        Tick old_when = executeInstructionEvent.when();
        DPRINTF(MAAIndirect, "I[%d] %s: execution already scheduled for tick %d\n", my_indirect_id, __func__, old_when);
        if (new_when < old_when) {
            DPRINTF(MAAIndirect, "I[%d] %s: rescheduling for tick %d!\n", my_indirect_id, __func__, new_when);
            maa->reschedule(executeInstructionEvent, new_when);
        }
    }
}
void IndirectAccessUnit::scheduleSendReadPacketEvent(int latency) {
    DPRINTF(MAAIndirect, "I[%d] %s: scheduling send read packet for the Indirect Unit in the next %d cycles!\n", my_indirect_id, __func__, latency);
    panic_if(latency < 0, "Negative latency of %d!\n", latency);
    Tick new_when = maa->getClockEdge(Cycles(latency));
    if (!sendReadPacketEvent.scheduled()) {
        maa->schedule(sendReadPacketEvent, new_when);
    } else {
        Tick old_when = sendReadPacketEvent.when();
        DPRINTF(MAAIndirect, "I[%d] %s: send packet already scheduled for tick %d\n", my_indirect_id, __func__, old_when);
        if (new_when < old_when) {
            DPRINTF(MAAIndirect, "I[%d] %s: rescheduling for tick %d!\n", my_indirect_id, __func__, new_when);
            maa->reschedule(sendReadPacketEvent, new_when);
        }
    }
}
void IndirectAccessUnit::scheduleSendWritePacketEvent(int latency) {
    DPRINTF(MAAIndirect, "I[%d] %s: scheduling send write packet for the Indirect Unit in the next %d cycles!\n", my_indirect_id, __func__, latency);
    panic_if(latency < 0, "Negative latency of %d!\n", latency);
    Tick new_when = maa->getClockEdge(Cycles(latency));
    if (!sendWritePacketEvent.scheduled()) {
        maa->schedule(sendWritePacketEvent, new_when);
    } else {
        Tick old_when = sendWritePacketEvent.when();
        DPRINTF(MAAIndirect, "I[%d] %s: send packet already scheduled for tick %d\n", my_indirect_id, __func__, old_when);
        if (new_when < old_when) {
            DPRINTF(MAAIndirect, "I[%d] %s: rescheduling for tick %d!\n", my_indirect_id, __func__, new_when);
            maa->reschedule(sendWritePacketEvent, new_when);
        }
    }
}

// bool IndirectAccessUnit::checkBlockCached(Addr physical_addr) {
//     RequestPtr real_req = std::make_shared<Request>(physical_addr,
//                                                     block_size,
//                                                     flags,
//                                                     maa->requestorId);
//     PacketPtr curr_pkt = new Packet(real_req, MemCmd::CleanEvict);
//     curr_pkt->setExpressSnoop();
//     curr_pkt->headerDelay = curr_pkt->payloadDelay = 0;
//     DPRINTF(MAAIndirect, "I[%d] %s: sending snoop of %s\n", my_indirect_id, __func__, curr_pkt->print());
//     maa->cpuSidePort.sendTimingSnoopReq(curr_pkt);
//     // assert(curr_pkt->satisfied() == false);
//     DPRINTF(MAAIndirect, "I[%d] %s: Snoop of %s returned with cache responding (%s), has sharers (%s), had writable (%s), satisfied (%s), is block cached (%s)\n",
//             my_indirect_id,
//             __func__,
//             curr_pkt->print(),
//             curr_pkt->cacheResponding() ? "True" : "False",
//             curr_pkt->hasSharers() ? "True" : "False",
//             curr_pkt->responderHadWritable() ? "True" : "False",
//             curr_pkt->satisfied() ? "True" : "False",
//             curr_pkt->isBlockCached() ? "True" : "False");
//     return curr_pkt->isBlockCached();
// }
} // namespace gem5