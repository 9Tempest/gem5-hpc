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

#ifndef TRACING_ON
#define TRACING_ON 1
#endif

namespace gem5 {

///////////////
//
// OFFSET TABLE
//
///////////////
void OffsetTable::allocate(int _num_tile_elements) {
    num_tile_elements = _num_tile_elements;
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
}
std::vector<OffsetTableEntry> OffsetTable::get_entry_recv(int first_itr) {
    std::vector<OffsetTableEntry> result;
    assert(first_itr != -1);
    int itr = first_itr;
    int prev_itr = itr;
    while (itr != -1) {
        result.push_back(entries[itr]);
        assert(entries_valid[itr] == true);
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
bool RowTableEntry::insert(Addr addr,
                           int itr,
                           int wid) {
    int free_entry_id = -1;
    for (int i = 0; i < num_row_table_entries_per_row; i++) {
        if (entries_valid[i] == true && entries[i].addr == addr) {
            offset_table->insert(itr, wid, entries[i].last_itr);
            entries[i].last_itr = itr;
            DPRINTF(MAAIndirect, "I[%d] T[%d] R[%d] %s: entry %d inserted!\n",
                    my_indirect_id, my_table_id, my_table_row_id, __func__,
                    i);
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
    entries[free_entry_id].is_cached = true; // indir_access->checkBlockCached(addr);
    entries_valid[free_entry_id] = true;
    offset_table->insert(itr, wid, -1);
    DPRINTF(MAAIndirect, "I[%d] T[%d] R[%d] %s: new entry %d [addr(0x%lx)] inserted!\n",
            my_indirect_id, my_table_id, my_table_row_id, __func__,
            free_entry_id,
            addr);
    return true;
}
void RowTableEntry::check_reset() {
    for (int i = 0; i < num_row_table_entries_per_row; i++) {
        panic_if(entries_valid[i], "Entry %d is valid: addr(0x%lx)!\n",
                 i, entries[i].addr);
    }
    panic_if(last_sent_entry_id != 0, "Last sent entry id is not 0: %d!\n",
             last_sent_entry_id);
}
void RowTableEntry::reset() {
    for (int i = 0; i < num_row_table_entries_per_row; i++) {
        entries_valid[i] = false;
    }
    last_sent_entry_id = 0;
}
bool RowTableEntry::get_entry_send(Addr &addr,
                                   bool &is_block_cached) {
    assert(last_sent_entry_id <= num_row_table_entries_per_row);
    for (; last_sent_entry_id < num_row_table_entries_per_row; last_sent_entry_id++) {
        if (entries_valid[last_sent_entry_id] == true) {
            addr = entries[last_sent_entry_id].addr;
            is_block_cached = entries[last_sent_entry_id].is_cached;
            DPRINTF(MAAIndirect, "I[%d] T[%d] R[%d] %s: sending entry %d [addr(0x%lx)]!\n",
                    my_indirect_id, my_table_id, my_table_row_id, __func__,
                    last_sent_entry_id, addr);
            last_sent_entry_id++;
            return true;
        }
    }
    return false;
}
std::vector<OffsetTableEntry> RowTableEntry::get_entry_recv(Addr addr,
                                                            bool is_block_cached) {
    for (int i = 0; i < num_row_table_entries_per_row; i++) {
        if (entries_valid[i] == true && entries[i].addr == addr) {
            entries_valid[i] = false;
            DPRINTF(MAAIndirect, "I[%d] T[%d] R[%d] %s: entry %d received, setting to invalid!\n",
                    my_indirect_id, my_table_id, my_table_row_id, __func__, i);
            // assert(entries[i].is_cached == is_block_cached);
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
bool RowTable::insert(Addr Grow_addr,
                      Addr addr,
                      int itr,
                      int wid) {
    int free_row_id = -1;
    for (int i = 0; i < num_row_table_rows; i++) {
        if (entries_valid[i] == true && entries[i].Grow_addr == Grow_addr) {
            if (entries[i].insert(addr, itr, wid)) {
                DPRINTF(MAAIndirect, "I[%d] T[%d] %s: Grow[0x%lx] inserted in row[%d]!\n",
                        my_indirect_id,
                        my_table_id,
                        __func__,
                        Grow_addr,
                        i);
                return true;
            }
        } else if (entries_valid[i] == false && free_row_id == -1) {
            free_row_id = i;
        }
    }
    if (free_row_id == -1) {
        return false;
        printf("I[%d] T[%d] Error: no free row id found for addr(0x%lx) and Grow_addr(0x%lx)!\n",
               my_indirect_id, my_table_id, addr, Grow_addr);
        for (int i = 0; i < num_row_table_rows; i++) {
            printf("Row[%d]: Grow_addr(0x%lx) Valid (%s), entries:",
                   i, entries[i].Grow_addr, entries_valid[i] ? "T" : "F");
            for (int j = 0; j < num_row_table_entries_per_row; j++) {
                printf("  (%d: %s)", j, entries[i].entries_valid[j] ? "T" : "F");
            }
            printf("\n");
        }
        assert(false);
    }
    DPRINTF(MAAIndirect, "I[%d] T[%d] %s: Grow[0x%lx] adding to new row[%d]!\n",
            my_indirect_id, my_table_id,
            __func__,
            Grow_addr,
            free_row_id);
    entries[free_row_id].Grow_addr = Grow_addr;
    assert(entries[free_row_id].insert(addr, itr, wid) == true);
    entries_valid[free_row_id] = true;
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
bool RowTable::get_entry_send(Addr &addr,
                              bool &is_block_cached) {
    assert(last_sent_row_id <= num_row_table_rows);
    for (; last_sent_row_id < num_row_table_rows; last_sent_row_id++) {
        if (entries_valid[last_sent_row_id] &&
            entries[last_sent_row_id].get_entry_send(addr, is_block_cached)) {
            DPRINTF(MAAIndirect, "I[%d] T[%d] %s: row %d retuned!\n", my_indirect_id, my_table_id, __func__, last_sent_row_id);
            return true;
        }
    }
    last_sent_row_id = 0;
    return false;
}
bool RowTable::get_entry_send_first_row(Addr &addr,
                                        bool &is_block_cached) {
    // This function must be called only when blocked b/c of not entry available
    assert(entries_valid[0]);
    if (entries[0].get_entry_send(addr, is_block_cached)) {
        DPRINTF(MAAIndirect, "I[%d] T[%d] %s: retuned addr(0x%lx)!\n",
                my_indirect_id, my_table_id, __func__, addr);
        return true;
    }
    last_sent_row_id = 0;
    return false;
}
std::vector<OffsetTableEntry> RowTable::get_entry_recv(Addr Grow_addr,
                                                       Addr addr,
                                                       bool is_block_cached) {
    std::vector<OffsetTableEntry> results;
    for (int i = 0; i < num_row_table_rows; i++) {
        if (entries_valid[i] == true && entries[i].Grow_addr == Grow_addr) {
            DPRINTF(MAAIndirect, "I[%d] T[%d] %s: Grow[0x%lx] hit with row[%d]!\n",
                    my_indirect_id,
                    my_table_id,
                    __func__,
                    Grow_addr,
                    i);
            std::vector<OffsetTableEntry> result = entries[i].get_entry_recv(addr, is_block_cached);
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
    offset_table->allocate(num_tile_elements);
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
        my_max = maa->spd->getSize(my_idx_tile);
        panic_if(my_cond_tile != -1 && my_max != maa->spd->getSize(my_cond_tile),
                 "I[%d] %s: idx size(%d) != cond size(%d)!\n",
                 my_indirect_id, __func__, my_max, maa->spd->getSize(my_cond_tile));

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
        my_SPD_read_finish_tick = curTick();
        my_SPD_write_finish_tick = curTick();
        my_RT_access_finish_tick = curTick();

        // Setting the state of the instruction and stream unit
        my_instruction->state = Instruction::Status::Service;
        DPRINTF(MAAIndirect, "I[%d] %s: state set to Fill for request %s!\n", my_indirect_id, __func__, my_instruction->print());
        prev_state = Status::Decode;
        state = Status::Fill;
        [[fallthrough]];
    }
    case Status::Fill: {
        // Reordering the indices
        int num_spd_read_accesses = 0;
        int num_rowtable_accesses = 0;
        DPRINTF(MAAIndirect, "I[%d] %s: filling %s!\n", my_indirect_id, __func__, my_instruction->print());
        if (scheduleNextExecution()) {
            break;
        }
        for (; my_i < my_max; my_i++) {
            if (my_cond_tile != -1) {
                num_spd_read_accesses++;
            }
            if (my_cond_tile == -1 || maa->spd->getData<uint32_t>(my_cond_tile, my_i) != 0) {
                uint32_t idx = maa->spd->getData<uint32_t>(my_idx_tile, my_i);
                num_spd_read_accesses++;
                Addr vaddr = my_base_addr + word_size * idx;
                Addr block_vaddr = addrBlockAlign(vaddr, block_size);
                Addr paddr = translatePacket(block_vaddr);
                Addr block_paddr = addrBlockAlign(paddr, block_size);
                DPRINTF(MAAIndirect, "I[%d] %s: idx = %u, addr = 0x%lx!\n",
                        my_indirect_id, __func__, idx, block_paddr);
                uint16_t wid = (vaddr - block_vaddr) / word_size;
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
                    DPRINTF(MAAIndirect, "I[%d] %s: insertion failed due to no space, average entry/row = %.2f, number of SPD accesses: %d, switching to drain mode...\n",
                            my_indirect_id, __func__, row_table[my_row_table_idx].getAverageEntriesPerRow(), num_spd_read_accesses);
                    my_SPD_read_finish_tick = maa->getClockEdge(maa->spd->getDataLatency(num_spd_read_accesses));
                    my_RT_access_finish_tick = maa->getClockEdge(Cycles(num_rowtable_accesses * rowtable_latency));
                    scheduleNextExecution(true);
                    prev_state = Status::Fill;
                    state = Status::Drain;
                    return;
                }
            }
        }
        if (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD) {
            maa->spd->setSize(my_dst_tile, my_max);
        }
        DPRINTF(MAAIndirect, "I[%d] %s: state set to Build for %s!\n",
                my_indirect_id, __func__, my_instruction->print());
        my_SPD_read_finish_tick = maa->getClockEdge(maa->spd->getDataLatency(num_spd_read_accesses));
        my_RT_access_finish_tick = maa->getClockEdge(Cycles(num_rowtable_accesses * rowtable_latency));
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
        int num_rowtable_accesses = 0;
        Addr addr;
        bool is_cached;
        while (row_table[my_row_table_idx].get_entry_send_first_row(addr, is_cached)) {
            DPRINTF(MAAIndirect, "I[%d] %s: Creating packet for bank[%d], addr[0x%lx], is_cached[%s]!\n",
                    my_indirect_id, __func__, my_row_table_idx, addr, is_cached ? "T" : "F");
            my_expected_responses++;
            num_rowtable_accesses++;
            /********Changed********/
            createReadPacket(addr, num_rowtable_accesses * rowtable_latency); // , is_cached);
        }
        DPRINTF(MAAIndirect, "I[%d] %s: drain completed, going to the request state...\n", my_indirect_id, __func__);
        my_RT_access_finish_tick = maa->getClockEdge(Cycles(num_rowtable_accesses * rowtable_latency));
        prev_state = Status::Drain;
        state = Status::Request;
        // scheduleNextExecution();
        scheduleNextSendRead();
        break;
    }
    case Status::Build: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: Building %s requests!\n", my_indirect_id, __func__, my_instruction->print());
        if (scheduleNextExecution()) {
            break;
        }
        int last_row_table_sent = 0;
        int num_rowtable_accesses = 0;
        Addr addr;
        bool is_cached;
        while (true) {
            if (checkAllRowTablesSent()) {
                break;
            }
            for (; last_row_table_sent < num_row_table_banks; last_row_table_sent++) {
                int row_table_idx = my_row_table_bank_order[last_row_table_sent];
                assert(row_table_idx < num_row_table_banks);
                DPRINTF(MAAIndirect, "I[%d] %s: Checking row table bank[%d]!\n", my_indirect_id, __func__, row_table_idx);
                if (my_row_table_req_sent[row_table_idx] == false) {
                    if (row_table[row_table_idx].get_entry_send(addr, is_cached)) {
                        DPRINTF(MAAIndirect, "I[%d] %s: Creating packet for bank[%d], addr[0x%lx], is_cached[%s]!\n",
                                my_indirect_id, __func__, row_table_idx, addr, is_cached ? "T" : "F");
                        my_expected_responses++;
                        num_rowtable_accesses++;
                        createReadPacket(addr, num_rowtable_accesses * rowtable_latency);
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
        my_RT_access_finish_tick = maa->getClockEdge(Cycles(num_rowtable_accesses * rowtable_latency));
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
        panic_if(my_received_responses != my_expected_responses, "I[%d] %s: received_responses(%d) != sent_requests(%d)!\n",
                 my_indirect_id, __func__, my_received_responses, my_expected_responses);
        if (prev_state == Status::Drain) {
            DPRINTF(MAAIndirect, "I[%d] %s: request completed, returning to the fill stage...\n", my_indirect_id, __func__);
            prev_state = Status::Request;
            state = Status::Fill;
        } else {
            assert(prev_state == Status::Build);
            DPRINTF(MAAIndirect, "I[%d] %s: request completed, state set to respond %s!\n", my_indirect_id, __func__);
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
        DPRINTF(MAAIndirect, "I[%d] %s: state set to finish for request %s!\n", my_indirect_id, __func__, my_instruction->print());
        my_instruction->state = Instruction::Status::Finish;
        prev_state = Status::max;
        state = Status::Idle;
        check_reset();
        if (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD) {
            maa->spd->setReady(my_dst_tile);
            maa->finishInstruction(my_instruction, my_dst_tile);
        } else {
            maa->spd->setReady(my_src_tile);
            maa->finishInstruction(my_instruction);
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
bool IndirectAccessUnit::checkBlockCached(Addr physical_addr) {
    RequestPtr real_req = std::make_shared<Request>(physical_addr,
                                                    block_size,
                                                    flags,
                                                    maa->requestorId);
    PacketPtr curr_pkt = new Packet(real_req, MemCmd::CleanEvict);
    curr_pkt->setExpressSnoop();
    curr_pkt->headerDelay = curr_pkt->payloadDelay = 0;
    DPRINTF(MAAIndirect, "I[%d] %s: sending snoop of %s\n", my_indirect_id, __func__, curr_pkt->print());
    maa->cpuSidePort.sendTimingSnoopReq(curr_pkt);
    // assert(curr_pkt->satisfied() == false);
    DPRINTF(MAAIndirect, "I[%d] %s: Snoop of %s returned with cache responding (%s), has sharers (%s), had writable (%s), satisfied (%s), is block cached (%s)\n",
            my_indirect_id,
            __func__,
            curr_pkt->print(),
            curr_pkt->cacheResponding() ? "True" : "False",
            curr_pkt->hasSharers() ? "True" : "False",
            curr_pkt->responderHadWritable() ? "True" : "False",
            curr_pkt->satisfied() ? "True" : "False",
            curr_pkt->isBlockCached() ? "True" : "False");
    return curr_pkt->isBlockCached();
}
void IndirectAccessUnit::createReadPacket(Addr addr, int latency, bool is_cached) {
    /**** Packet generation ****/
    RequestPtr real_req = std::make_shared<Request>(addr, block_size, flags, maa->requestorId);
    PacketPtr read_pkt;
    /********Changed*******/
    // if (is_cached) {
    if (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD) {
        read_pkt = new Packet(real_req, MemCmd::ReadSharedReq);
    } else {
        read_pkt = new Packet(real_req, MemCmd::ReadExReq);
    }
    // } else {
    //     read_pkt = new Packet(real_req, MemCmd::ReadReq);
    // }
    read_pkt->allocate();
    IndirectAccessUnit::IndirectPacket new_packet = IndirectAccessUnit::IndirectPacket(read_pkt, is_cached, maa->getClockEdge(Cycles(latency)));
    my_outstanding_read_pkts.insert(new_packet);
    DPRINTF(MAAIndirect, "I[%d] %s: created %s to send in %d cycles at tick %u, is cached %s\n", my_indirect_id, __func__, read_pkt->print(), latency, new_packet.tick, is_cached ? "True" : "False");
}
bool IndirectAccessUnit::sendOutstandingReadPacket() {
    DPRINTF(MAAIndirect, "I[%d] %s: sending %d outstanding read packets...\n", my_indirect_id, __func__, my_outstanding_read_pkts.size());
    while (my_outstanding_read_pkts.empty() == false) {
        IndirectAccessUnit::IndirectPacket read_pkt = *my_outstanding_read_pkts.begin();
        DPRINTF(MAAIndirect, "I[%d] %s: trying sending %s to %s at time %u from %d packets\n",
                my_indirect_id, __func__, read_pkt.packet->print(), read_pkt.is_cached ? "cache" : "memory", read_pkt.tick, my_outstanding_read_pkts.size());
        if (read_pkt.tick > curTick()) {
            DPRINTF(MAAIndirect, "I[%d] %s: waiting for %d cycles\n",
                    my_indirect_id, __func__, maa->getTicksToCycles(read_pkt.tick - curTick()));
            scheduleNextSendRead();
            return false;
        }
        if (read_pkt.is_cached) {
            DPRINTF(MAAIndirect, "I[%d] %s: trying sending %s to cache\n", my_indirect_id, __func__, read_pkt.packet->print());
            PacketPtr new_read_pkt = new Packet(read_pkt.packet, true, false);
            new_read_pkt->setExpressSnoop();
            new_read_pkt->headerDelay = new_read_pkt->payloadDelay = 0;
            maa->cpuSidePort.sendTimingSnoopReq(new_read_pkt);
            DPRINTF(MAAIndirect, "I[%d] %s: successfully sent as a snoop to membus, cache responding: %s, has sharers %s, had writable %s, satisfied %s, is block cached %s...\n",
                    my_indirect_id,
                    __func__,
                    new_read_pkt->cacheResponding() ? "True" : "False",
                    new_read_pkt->hasSharers() ? "True" : "False",
                    new_read_pkt->responderHadWritable() ? "True" : "False",
                    new_read_pkt->satisfied() ? "True" : "False",
                    new_read_pkt->isBlockCached() ? "True" : "False");
            my_outstanding_read_pkts.erase(my_outstanding_read_pkts.begin());
            if (new_read_pkt->cacheResponding() == true) {
                DPRINTF(MAAIndirect, "I[%d] %s: a cache in the O/M state will respond, send successfull...\n", my_indirect_id, __func__);
            } else {
                DPRINTF(MAAIndirect, "I[%d] %s: no cache responds (I/S/E --> I), creating again and sending to memory\n", my_indirect_id, __func__);
                createReadPacket(read_pkt.packet->getAddr(), cache_snoop_latency, false);
            }
            // We can continue sending the next packets anyway
        } else {
            DPRINTF(MAAIndirect, "I[%d] %s: trying sending %s to memory\n", my_indirect_id, __func__, read_pkt.packet->print());
            if (maa->memSidePort.sendPacket(my_indirect_id,
                                            read_pkt.packet) == false) {
                DPRINTF(MAAIndirect, "I[%d] %s: send failed, leaving send packet...\n", my_indirect_id, __func__);
                // A send has failed, we cannot continue sending the next packets
                return false;
            } else {
                my_outstanding_read_pkts.erase(my_outstanding_read_pkts.begin());
                // A packet is sent successfully, we can continue sending the next packets
            }
        }
    }
    return true;
}
bool IndirectAccessUnit::sendOutstandingWritePacket() {
    while (my_outstanding_write_pkts.empty() == false) {
        IndirectAccessUnit::IndirectPacket write_pkt = *my_outstanding_write_pkts.begin();
        if (write_pkt.tick > curTick()) {
            DPRINTF(MAAIndirect, "I[%d] %s: waiting for %d cycles to send %s to memory\n",
                    my_indirect_id, __func__, maa->getTicksToCycles(write_pkt.tick - curTick()), write_pkt.packet->print());
            scheduleNextSendWrite();
            return false;
        }
        // panic_if(write_pkt.tick < curTick(), "I[%d] %s: latency (%d) is less than current tick (%d)!\n",
        //          my_indirect_id, __func__, write_pkt.tick, curTick());
        DPRINTF(MAAIndirect, "I[%d] %s: trying sending %s to memory\n", my_indirect_id, __func__, write_pkt.packet->print());
        if (maa->memSidePort.sendPacket(my_indirect_id,
                                        write_pkt.packet) == false) {
            DPRINTF(MAAIndirect, "I[%d] %s: send failed, leaving execution...\n", my_indirect_id, __func__);
            return false;
        } else {
            my_outstanding_write_pkts.erase(my_outstanding_write_pkts.begin());
            my_received_responses++;
        }
    }
    if (my_received_responses == my_expected_responses) {
        DPRINTF(MAAIndirect, "I[%d] %s: all responses received, calling execution again in state %s!\n",
                my_indirect_id, __func__, status_names[(int)state]);
        scheduleNextExecution(true);
    }
    return true;
}
bool IndirectAccessUnit::recvData(const Addr addr,
                                  uint8_t *dataptr,
                                  std::vector<uint32_t> data,
                                  std::vector<uint16_t> wids,
                                  bool is_block_cached) {
    std::vector addr_vec = maa->map_addr(addr);
    Addr Grow_addr = maa->calc_Grow_addr(addr_vec);
    int row_table_idx = getRowTableIdx(addr_vec[ADDR_CHANNEL_LEVEL],
                                       addr_vec[ADDR_RANK_LEVEL],
                                       addr_vec[ADDR_BANKGROUP_LEVEL]);
    std::vector<OffsetTableEntry> entries = row_table[row_table_idx].get_entry_recv(Grow_addr,
                                                                                    addr,
                                                                                    is_block_cached);
    DPRINTF(MAAIndirect, "I[%d] %s: %d entries received for addr(0x%lx), Grow(x%lx) from T[%d]!\n",
            my_indirect_id, __func__, entries.size(),
            addr, Grow_addr, row_table_idx);
    if (entries.size() == 0) {
        return false;
    }
    uint8_t new_data[block_size];
    std::memcpy(new_data, dataptr, block_size);
    assert(wids.size() == block_size / word_size);
    for (int i = 0; i < block_size / word_size; i++) {
        assert(wids[i] == i);
    }
    int num_recv_spd_read_accesses = 0;
    int num_recv_spd_write_accesses = 0;
    int num_recv_rt_accesses = entries.size();
    for (auto entry : entries) {
        int itr = entry.itr;
        int wid = entry.wid;
        DPRINTF(MAAIndirect, "I[%d] %s: itr (%d) wid (%d) matched!\n", my_indirect_id, __func__, itr, wid);
        switch (my_instruction->opcode) {
        case Instruction::OpcodeType::INDIR_LD: {
            maa->spd->setData<uint32_t>(my_dst_tile, itr, data[wid]);
            num_recv_spd_write_accesses++;
            break;
        }
        case Instruction::OpcodeType::INDIR_ST: {
            ((uint32_t *)new_data)[wid] = maa->spd->getData<uint32_t>(my_src_tile, itr);
            num_recv_spd_read_accesses++;
            DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] = SPD[%d][%d] = %f!\n",
                    my_indirect_id, __func__, wid, my_src_tile, itr, ((float *)new_data)[wid]);
            break;
        }
        case Instruction::OpcodeType::INDIR_RMW: {
            switch (my_instruction->datatype) {
            case Instruction::DataType::INT32_TYPE: {
                int32_t word_data = maa->spd->getData<int32_t>(my_src_tile, itr);
                num_recv_spd_read_accesses++;
                assert(my_instruction->optype == Instruction::OPType::ADD_OP);
                DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] (%d) += SPD[%d][%d] (%d) = %d!\n",
                        my_indirect_id, __func__, wid, ((int *)new_data)[wid], my_src_tile, itr, word_data, ((int *)new_data)[wid] + word_data);
                ((int32_t *)new_data)[wid] += word_data;
                break;
            }
            case Instruction::DataType::FLOAT32_TYPE: {
                float word_data = maa->spd->getData<float>(my_src_tile, itr);
                num_recv_spd_read_accesses++;
                assert(my_instruction->optype == Instruction::OPType::ADD_OP);
                DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] (%f) += SPD[%d][%d] (%f) = %f!\n",
                        my_indirect_id, __func__, wid, ((float *)new_data)[wid], my_src_tile, itr, word_data, ((float *)new_data)[wid] + word_data);
                ((float *)new_data)[wid] += word_data;
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
    Cycles get_data_latency = maa->spd->getDataLatency(num_recv_spd_write_accesses);
    my_SPD_read_finish_tick = maa->getClockEdge(get_data_latency);
    Cycles set_data_latency = maa->spd->setDataLatency(num_recv_spd_read_accesses);
    my_SPD_write_finish_tick = maa->getClockEdge(set_data_latency);
    Cycles access_rt_latency = Cycles(num_recv_rt_accesses * rowtable_latency);
    if (my_RT_access_finish_tick < curTick())
        my_RT_access_finish_tick = maa->getClockEdge(access_rt_latency);
    else
        my_RT_access_finish_tick += maa->getCyclesToTicks(access_rt_latency);
    if (my_instruction->opcode == Instruction::OpcodeType::INDIR_ST || my_instruction->opcode == Instruction::OpcodeType::INDIR_RMW) {
        Cycles total_latency = std::max(std::max(set_data_latency, get_data_latency), access_rt_latency);
        RequestPtr real_req = std::make_shared<Request>(addr,
                                                        block_size,
                                                        flags,
                                                        maa->requestorId);
        PacketPtr write_pkt = new Packet(real_req, MemCmd::WritebackDirty);
        write_pkt->allocate();
        write_pkt->setData(new_data);
        for (int i = 0; i < block_size / word_size; i++) {
            DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] = %f!\n",
                    my_indirect_id, __func__, i, write_pkt->getPtr<float>()[i]);
        }
        DPRINTF(MAAIndirect, "I[%d] %s: created %s to send in %d cycles\n", my_indirect_id, __func__, write_pkt->print(), total_latency);
        my_outstanding_write_pkts.insert(IndirectAccessUnit::IndirectPacket(write_pkt, false, maa->getClockEdge(Cycles(total_latency))));
        scheduleNextSendWrite();
    } else {
        my_received_responses++;
        if (my_received_responses == my_expected_responses) {
            DPRINTF(MAAIndirect, "I[%d] %s: all responses received, calling execution again!\n", my_indirect_id, __func__);
            scheduleNextExecution(true);
        } else {
            DPRINTF(MAAIndirect, "I[%d] %s: expected: %d, received: %d responses!\n", my_indirect_id, __func__, my_expected_responses, my_received_responses);
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

// IndirectAccessUnit::IndirectAccessUnitStats::IndirectAccessUnitStats(statistics::Group *parent)
//     : statistics::Group(parent),
//       ADD_STAT(numInstRD, statistics::units::Count::get(),
//                "number of INDIRECT_LD instructions"),
//       ADD_STAT(numInstWR, statistics::units::Count::get(),
//                "number of INDIRECT_ST instructions"),
//       ADD_STAT(numInstRMW, statistics::units::Count::get(),
//                "number of INDIRECT_RMW instructions"),
//       ADD_STAT(numInst, statistics::units::Count::get(),
//                "number of instructions"),
//       ADD_STAT(cyclesRD, statistics::units::Count::get(),
//                "number of INDIRECT_LD cycles"),
//       ADD_STAT(cyclesWR, statistics::units::Count::get(),
//                "number of INDIRECT_ST cycles"),
//       ADD_STAT(cyclesRMW, statistics::units::Count::get(),
//                "number of INDIRECT_RMW cycles"),
//       ADD_STAT(cycles, statistics::units::Count::get(),
//                "number of cycles") {
//     using namespace statistics;
//     avgCPIRD = cyclesRD / numInstRD;
//     avgCPIWR = cyclesWR / numInstWR;
//     avgCPIRMW = cyclesRMW / numInstRMW;
//     avgCPI = cycles / numInst;
// }

// void IndirectAccessUnit::IndirectAccessUnitStats::regStats() {
//     using namespace statistics;

//     statistics::Group::regStats();

//     System *system = cache.system;
//     const auto max_requestors = system->maxRequestors();

//     for (auto &cs : cmd)
//         cs->regStatsFromParent();

//     for (int idx = 0; idx < MAX_CMD_REGIONS; ++idx) {
//         for (auto &cs : cmdRegions[idx])
//             cs->regStatsFromParent();
//     }

// // These macros make it easier to sum the right subset of commands and
// // to change the subset of commands that are considered "demand" vs
// // "non-demand"
// #define SUM_DEMAND(s)                                           \
//     (cmd[MemCmd::ReadReq]->s + cmd[MemCmd::WriteReq]->s +       \
//      cmd[MemCmd::WriteLineReq]->s + cmd[MemCmd::ReadExReq]->s + \
//      cmd[MemCmd::ReadCleanReq]->s + cmd[MemCmd::ReadSharedReq]->s)

// #define SUM_DEMAND_REGION(s)                                                            \
//     (cmdRegions[idx][MemCmd::ReadReq]->s + cmdRegions[idx][MemCmd::WriteReq]->s +       \
//      cmdRegions[idx][MemCmd::WriteLineReq]->s + cmdRegions[idx][MemCmd::ReadExReq]->s + \
//      cmdRegions[idx][MemCmd::ReadCleanReq]->s + cmdRegions[idx][MemCmd::ReadSharedReq]->s)

// // should writebacks be included here?  prior code was inconsistent...
// #define SUM_NON_DEMAND(s)                                    \
//     (cmd[MemCmd::SoftPFReq]->s + cmd[MemCmd::HardPFReq]->s + \
//      cmd[MemCmd::SoftPFExReq]->s)

// #define SUM_NON_DEMAND_REGION(s)                                                     \
//     (cmdRegions[idx][MemCmd::SoftPFReq]->s + cmdRegions[idx][MemCmd::HardPFReq]->s + \
//      cmdRegions[idx][MemCmd::SoftPFExReq]->s)

//     for (int idx = 0; idx < MAX_CMD_REGIONS + 1; ++idx) {
//         // printf("Registering demandHits\n");
//         (*demandHits[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*demandHits[idx]) = SUM_DEMAND(hits);
//         } else {
//             (*demandHits[idx]) = SUM_DEMAND_REGION(hits);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*demandHits[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallHits\n");
//         (*overallHits[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*overallHits[idx]) = (*demandHits[idx]) + SUM_NON_DEMAND(hits);
//         } else {
//             (*overallHits[idx]) = (*demandHits[idx]) + SUM_NON_DEMAND_REGION(hits);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallHits[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering demandMisses\n");
//         (*demandMisses[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*demandMisses[idx]) = SUM_DEMAND(misses);
//         } else {
//             (*demandMisses[idx]) = SUM_DEMAND_REGION(misses);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*demandMisses[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallMisses\n");
//         (*overallMisses[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*overallMisses[idx]) = (*demandMisses[idx]) + SUM_NON_DEMAND(misses);
//         } else {
//             (*overallMisses[idx]) = (*demandMisses[idx]) + SUM_NON_DEMAND_REGION(misses);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallMisses[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering demandMissLatency\n");
//         (*demandMissLatency[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*demandMissLatency[idx]) = SUM_DEMAND(missLatency);
//         } else {
//             (*demandMissLatency[idx]) = SUM_DEMAND_REGION(missLatency);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*demandMissLatency[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallMissLatency\n");
//         (*overallMissLatency[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*overallMissLatency[idx]) = (*demandMissLatency[idx]) + SUM_NON_DEMAND(missLatency);
//         } else {
//             (*overallMissLatency[idx]) = (*demandMissLatency[idx]) + SUM_NON_DEMAND_REGION(missLatency);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallMissLatency[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering demandHitLatency\n");
//         (*demandHitLatency[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*demandHitLatency[idx]) = SUM_DEMAND(hitLatency);
//         } else {
//             (*demandHitLatency[idx]) = SUM_DEMAND_REGION(hitLatency);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*demandHitLatency[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallHitLatency\n");
//         (*overallHitLatency[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*overallHitLatency[idx]) = (*demandHitLatency[idx]) + SUM_NON_DEMAND(hitLatency);
//         } else {
//             (*overallHitLatency[idx]) = (*demandHitLatency[idx]) + SUM_NON_DEMAND_REGION(hitLatency);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallHitLatency[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering demandAccesses\n");
//         (*demandAccesses[idx]).flags(total | nozero | nonan);
//         (*demandAccesses[idx]) = (*demandHits[idx]) + (*demandMisses[idx]);
//         for (int i = 0; i < max_requestors; i++) {
//             (*demandAccesses[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallAccesses\n");
//         (*overallAccesses[idx]).flags(total | nozero | nonan);
//         (*overallAccesses[idx]) = (*overallHits[idx]) + (*overallMisses[idx]);
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallAccesses[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering demandMissRate\n");
//         (*demandMissRate[idx]).flags(total | nozero | nonan);
//         (*demandMissRate[idx]) = (*demandMisses[idx]) / (*demandAccesses[idx]);
//         for (int i = 0; i < max_requestors; i++) {
//             (*demandMissRate[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallMissRate\n");
//         (*overallMissRate[idx]).flags(total | nozero | nonan);
//         (*overallMissRate[idx]) = (*overallMisses[idx]) / (*overallAccesses[idx]);
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallMissRate[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering demandAvgMissLatency\n");
//         (*demandAvgMissLatency[idx]).flags(total | nozero | nonan);
//         (*demandAvgMissLatency[idx]) = (*demandMissLatency[idx]) / (*demandMisses[idx]);
//         for (int i = 0; i < max_requestors; i++) {
//             (*demandAvgMissLatency[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallAvgMissLatency\n");
//         (*overallAvgMissLatency[idx]).flags(total | nozero | nonan);
//         (*overallAvgMissLatency[idx]) = (*overallMissLatency[idx]) / (*overallMisses[idx]);
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallAvgMissLatency[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering blockedCycles\n");
//         (*blockedCycles[idx]).init(NUM_BLOCKED_CAUSES).flags(total | nozero | nonan);
//         (*blockedCycles[idx]).subname(Blocked_NoMSHRs, "no_mshrs").subname(Blocked_NoTargets, "no_targets");

//         // printf("Registering blockedCauses\n");
//         (*blockedCauses[idx]).init(NUM_BLOCKED_CAUSES).flags(total | nozero | nonan);
//         (*blockedCauses[idx]).subname(Blocked_NoMSHRs, "no_mshrs").subname(Blocked_NoTargets, "no_targets");

//         // printf("Registering avgBlocked\n");
//         (*avgBlocked[idx]).flags(total | nozero | nonan);
//         (*avgBlocked[idx]).subname(Blocked_NoMSHRs, "no_mshrs").subname(Blocked_NoTargets, "no_targets");
//         (*avgBlocked[idx]) = (*blockedCycles[idx]) / (*blockedCauses[idx]);

//         // printf("Registering writebacks\n");
//         (*writebacks[idx]).init(max_requestors).flags(total | nozero | nonan);
//         for (int i = 0; i < max_requestors; i++) {
//             (*writebacks[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering demandMshrHits\n");
//         (*demandMshrHits[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*demandMshrHits[idx]) = SUM_DEMAND(mshrHits);
//         } else {
//             (*demandMshrHits[idx]) = SUM_DEMAND_REGION(mshrHits);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*demandMshrHits[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallMshrHits\n");
//         (*overallMshrHits[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*overallMshrHits[idx]) = (*demandMshrHits[idx]) + SUM_NON_DEMAND(mshrHits);
//         } else {
//             (*overallMshrHits[idx]) = (*demandMshrHits[idx]) + SUM_NON_DEMAND_REGION(mshrHits);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallMshrHits[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering demandMshrMisses\n");
//         (*demandMshrMisses[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*demandMshrMisses[idx]) = SUM_DEMAND(mshrMisses);
//         } else {
//             (*demandMshrMisses[idx]) = SUM_DEMAND_REGION(mshrMisses);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*demandMshrMisses[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallMshrMisses\n");
//         (*overallMshrMisses[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*overallMshrMisses[idx]) = (*demandMshrMisses[idx]) + SUM_NON_DEMAND(mshrMisses);
//         } else {
//             (*overallMshrMisses[idx]) = (*demandMshrMisses[idx]) + SUM_NON_DEMAND_REGION(mshrMisses);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallMshrMisses[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering demandMshrMissLatency\n");
//         (*demandMshrMissLatency[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*demandMshrMissLatency[idx]) = SUM_DEMAND(mshrMissLatency);
//         } else {
//             (*demandMshrMissLatency[idx]) = SUM_DEMAND_REGION(mshrMissLatency);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*demandMshrMissLatency[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallMshrMissLatency\n");
//         (*overallMshrMissLatency[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*overallMshrMissLatency[idx]) = (*demandMshrMissLatency[idx]) + SUM_NON_DEMAND(mshrMissLatency);
//         } else {
//             (*overallMshrMissLatency[idx]) = (*demandMshrMissLatency[idx]) + SUM_NON_DEMAND_REGION(mshrMissLatency);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallMshrMissLatency[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallMshrUncacheable\n");
//         (*overallMshrUncacheable[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*overallMshrUncacheable[idx]) = SUM_DEMAND(mshrUncacheable) + SUM_NON_DEMAND(mshrUncacheable);
//         } else {
//             (*overallMshrUncacheable[idx]) = SUM_DEMAND_REGION(mshrUncacheable) + SUM_NON_DEMAND_REGION(mshrUncacheable);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallMshrUncacheable[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallMshrUncacheableLatency\n");
//         (*overallMshrUncacheableLatency[idx]).flags(total | nozero | nonan);
//         if (idx == MAX_CMD_REGIONS) {
//             (*overallMshrUncacheableLatency[idx]) = SUM_DEMAND(mshrUncacheableLatency) + SUM_NON_DEMAND(mshrUncacheableLatency);
//         } else {
//             (*overallMshrUncacheableLatency[idx]) = SUM_DEMAND_REGION(mshrUncacheableLatency) + SUM_NON_DEMAND_REGION(mshrUncacheableLatency);
//         }
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallMshrUncacheableLatency[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering demandMshrMissRate\n");
//         (*demandMshrMissRate[idx]).flags(total | nozero | nonan);
//         (*demandMshrMissRate[idx]) = (*demandMshrMisses[idx]) / (*demandAccesses[idx]);
//         for (int i = 0; i < max_requestors; i++) {
//             (*demandMshrMissRate[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallMshrMissRate\n");
//         (*overallMshrMissRate[idx]).flags(total | nozero | nonan);
//         (*overallMshrMissRate[idx]) = (*overallMshrMisses[idx]) / (*overallAccesses[idx]);
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallMshrMissRate[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering demandAvgMshrMissLatency\n");
//         (*demandAvgMshrMissLatency[idx]).flags(total | nozero | nonan);
//         (*demandAvgMshrMissLatency[idx]) = (*demandMshrMissLatency[idx]) / (*demandMshrMisses[idx]);
//         for (int i = 0; i < max_requestors; i++) {
//             (*demandAvgMshrMissLatency[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallAvgMshrMissLatency\n");
//         (*overallAvgMshrMissLatency[idx]).flags(total | nozero | nonan);
//         (*overallAvgMshrMissLatency[idx]) = (*overallMshrMissLatency[idx]) / (*overallMshrMisses[idx]);
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallAvgMshrMissLatency[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering overallAvgMshrUncacheableLatency\n");
//         (*overallAvgMshrUncacheableLatency[idx]).flags(total | nozero | nonan);
//         (*overallAvgMshrUncacheableLatency[idx]) = (*overallMshrUncacheableLatency[idx]) / (*overallMshrUncacheable[idx]);
//         for (int i = 0; i < max_requestors; i++) {
//             (*overallAvgMshrUncacheableLatency[idx]).subname(i, system->getRequestorName(i));
//         }

//         // printf("Registering replacements\n");
//         (*replacements[idx]).flags(nozero | nonan);
//         // printf("Registering dataExpansions\n");
//         (*dataExpansions[idx]).flags(nozero | nonan);
//         // printf("Registering dataContractions\n");
//         (*dataContractions[idx]).flags(nozero | nonan);
//     }
// }
} // namespace gem5