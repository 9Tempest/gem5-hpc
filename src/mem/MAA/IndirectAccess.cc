#include "mem/MAA/IndirectAccess.hh"
#include "base/trace.hh"
#include "mem/MAA/MAA.hh"
#include "debug/MAAIndirect.hh"
#include <cassert>

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
    while (itr != -1) {
        result.push_back(entries[itr]);
        assert(entries_valid[itr] == true);
        entries_valid[itr] = false;
        itr = entries[itr].next_itr;
    }
    return result;
}
void OffsetTable::reset() {
    for (int i = 0; i < num_tile_elements; i++) {
        entries_valid[i] = false;
        entries[i].next_itr = -1;
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
    entries[free_entry_id].is_cached = indir_access->checkBlockCached(addr);
    entries_valid[free_entry_id] = true;
    offset_table->insert(itr, wid, -1);
    DPRINTF(MAAIndirect, "I[%d] T[%d] R[%d] %s: new entry %d [addr(0x%lx)] inserted!\n",
            my_indirect_id, my_table_id, my_table_row_id, __func__,
            free_entry_id,
            addr);
    // if (entries[free_entry_id].is_cached) {
    //     DPRINTF(MAAIndirect, "I[%d] T[%d] R[%d] %s: entry is cached, draining...\n",
    //             my_indirect_id, my_table_id, my_table_row_id, __func__);
    //     indir_access->createMyPacket();
    //     indir_access->sendOutstandingPacket();
    // }
    return true;
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
            assert(entries[i].is_cached == is_block_cached);
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
                entries[i].reset();
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
    : executeInstructionEvent([this] { executeInstruction(); }, name()) {
    row_table = nullptr;
}
void IndirectAccessUnit::allocate(int _my_indirect_id,
                                  int _num_tile_elements,
                                  int _num_row_table_rows,
                                  int _num_row_table_entries_per_row,
                                  MAA *_maa) {
    my_indirect_id = _my_indirect_id;
    maa = _maa;
    num_tile_elements = _num_tile_elements;
    num_row_table_rows = _num_row_table_rows;
    num_row_table_entries_per_row = _num_row_table_entries_per_row;
    num_row_table_banks = maa->m_org[ADDR_CHANNEL_LEVEL] *
                          maa->m_org[ADDR_RANK_LEVEL] * 2;
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
    my_all_row_table_req_sent = false;
    translation_done = false;
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
void IndirectAccessUnit::executeInstruction() {
    switch (state) {
    case Status::Idle: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: idling %s!\n", my_indirect_id, __func__, my_instruction->print());
        state = Status::Decode;
        [[fallthrough]];
    }
    case Status::Decode: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: decoding %s!\n", my_indirect_id, __func__, my_instruction->print());

        // Decoding the instruction
        my_idx_tile = my_instruction->src1SpdID;
        my_dst_tile = my_instruction->dst1SpdID;
        my_cond_tile = my_instruction->condSpdID;
        my_max = maa->spd->getSize(my_idx_tile);

        // Initialization
        my_virtual_addr = 0;
        my_base_addr = my_instruction->baseAddr;
        my_outstanding_pkt = false;
        my_received_responses = my_expected_responses = 0;
        my_is_block_cached = true;
        my_last_row_table_sent = 0;
        for (int i = 0; i < num_row_table_banks; i++) {
            row_table[i].reset();
            my_row_table_req_sent[i] = false;
        }
        my_i = 0;
        DPRINTF(MAAIndirect, "I[%d] %s: state set to Fill for request %s!\n", my_indirect_id, __func__, my_instruction->print());
        state = Status::Fill;
        [[fallthrough]];
    }
    case Status::Fill: {
        // Reordering the indices
        DPRINTF(MAAIndirect, "I[%d] %s: filling %s!\n", my_indirect_id, __func__, my_instruction->print());
        my_all_row_table_req_sent = false;
        for (; my_i < my_max; my_i++) {
            if (my_cond_tile == -1 || maa->spd->getData(my_cond_tile, my_i) != 0) {
                uint32_t idx = maa->spd->getData(my_idx_tile, my_i);
                Addr curr_addr = my_base_addr + word_size * idx;
                my_virtual_addr = addrBlockAlign(curr_addr, block_size);
                translatePacket();
                my_translated_block_physical_address = addrBlockAlign(my_translated_physical_address, block_size);
                uint16_t wid = (curr_addr - my_virtual_addr) / word_size;
                std::vector<int> addr_vec = maa->map_addr(my_translated_block_physical_address);
                Addr Grow_addr = maa->calc_Grow_addr(addr_vec);
                my_row_table_idx = getRowTableIdx(addr_vec[ADDR_CHANNEL_LEVEL],
                                                  addr_vec[ADDR_RANK_LEVEL],
                                                  addr_vec[ADDR_BANKGROUP_LEVEL]);
                assert(my_row_table_idx < num_row_table_banks);
                DPRINTF(MAAIndirect, "I[%d] %s: inserting vaddr(0x%lx), paddr(0x%lx), MAP(RO: %d, BA: %d, BG: %d, RA: %d, CO: %d, CH: %d), Grow(0x%lx), itr(%d), idx(%d), wid(%d) to T[%d]\n",
                        my_indirect_id, __func__,
                        my_virtual_addr, my_translated_block_physical_address,
                        addr_vec[ADDR_ROW_LEVEL], addr_vec[ADDR_BANK_LEVEL], addr_vec[ADDR_BANKGROUP_LEVEL], addr_vec[ADDR_RANK_LEVEL], addr_vec[ADDR_COLUMN_LEVEL], addr_vec[ADDR_CHANNEL_LEVEL],
                        Grow_addr, my_i, idx, wid, my_row_table_idx);
                bool inserted = row_table[my_row_table_idx].insert(Grow_addr, my_translated_block_physical_address, my_i, wid);
                if (inserted == false) {
                    DPRINTF(MAAIndirect, "I[%d] %s: insertion failed due to no space, average entry/row = %.2f, switching to drain mode...\n",
                            my_indirect_id, __func__, row_table[my_row_table_idx].getAverageEntriesPerRow());
                    state = Status::Drain;
                    scheduleExecuteInstructionEvent();
                    return;
                }
            }
        }
        maa->spd->setSize(my_dst_tile, my_max);
        // Setting the state of the instruction and stream unit
        my_instruction->state = Instruction::Status::Service;
        DPRINTF(MAAIndirect, "I[%d] %s: state set to Request for request %s!\n", my_indirect_id, __func__, my_instruction->print());
        state = Status::Request;
        scheduleExecuteInstructionEvent();
        return;
    }
    case Status::Drain: {
        DPRINTF(MAAIndirect, "I[%d] %s: draining %s!\n", my_indirect_id, __func__, my_instruction->print());
        if (my_outstanding_pkt) {
            if (sendOutstandingPacket() == false) {
                break;
            }
        }
        while (my_all_row_table_req_sent == false) {
            if (row_table[my_row_table_idx].get_entry_send_first_row(my_translated_block_physical_address, my_is_block_cached)) {
                DPRINTF(MAAIndirect, "I[%d] %s: Creating packet for bank[%d], addr[0x%lx], is_cached[%s]!\n",
                        my_indirect_id,
                        __func__,
                        my_row_table_idx,
                        my_translated_block_physical_address,
                        my_is_block_cached ? "T" : "F");
                my_expected_responses++;
                createMyPacket();
                if (sendOutstandingPacket() == false) {
                    break;
                }
            } else {
                DPRINTF(MAAIndirect, "I[%d] %s: T[%d] has nothing, setting sent to true!\n", my_indirect_id, __func__, my_row_table_idx);
                my_all_row_table_req_sent = true;
            }
        }
        if ((my_all_row_table_req_sent == true) && (my_received_responses == my_expected_responses)) {
            DPRINTF(MAAIndirect, "I[%d] %s: drain completed, returning to the fill stage...\n", my_indirect_id, __func__);
            state = Status::Fill;
            scheduleExecuteInstructionEvent();
        }
        break;
    }
    case Status::Request: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: requesting %s!\n", my_indirect_id, __func__, my_instruction->print());
        if (my_outstanding_pkt) {
            sendOutstandingPacket();
        }
        while (true) {
            checkAllRowTablesSent();
            if (my_outstanding_pkt || my_all_row_table_req_sent) {
                break;
            }
            for (; my_last_row_table_sent < num_row_table_banks; my_last_row_table_sent++) {
                int row_table_idx = my_row_table_bank_order[my_last_row_table_sent];
                assert(row_table_idx < num_row_table_banks);
                DPRINTF(MAAIndirect, "I[%d] %s: Checking row table bank[%d]!\n", my_indirect_id, __func__, row_table_idx);
                if (my_row_table_req_sent[row_table_idx] == false) {
                    if (row_table[row_table_idx].get_entry_send(my_translated_block_physical_address, my_is_block_cached)) {
                        DPRINTF(MAAIndirect, "I[%d] %s: Creating packet for bank[%d], addr[0x%lx], is_cached[%s]!\n",
                                my_indirect_id,
                                __func__,
                                row_table_idx,
                                my_translated_block_physical_address,
                                my_is_block_cached ? "T" : "F");
                        my_expected_responses++;
                        createMyPacket();
                        if (sendOutstandingPacket() == false) {
                            my_last_row_table_sent++;
                            break;
                        }
                    } else {
                        DPRINTF(MAAIndirect, "I[%d] %s: T[%d] has nothing, setting sent to true!\n", my_indirect_id, __func__, row_table_idx);
                        my_row_table_req_sent[row_table_idx] = true;
                    }
                } else {
                    DPRINTF(MAAIndirect, "I[%d] %s: T[%d] has already sent the requests!\n", my_indirect_id, __func__, row_table_idx);
                }
            }
            my_last_row_table_sent = (my_last_row_table_sent >= num_row_table_banks) ? 0 : my_last_row_table_sent;
        }
        if (my_outstanding_pkt) {
            break;
        }
        DPRINTF(MAAIndirect, "I[%d] %s: state set to respond for request %s!\n", my_indirect_id, __func__, my_instruction->print());
        state = Status::Response;
        [[fallthrough]];
    }
    case Status::Response: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: responding %s!\n", my_indirect_id, __func__, my_instruction->print());
        if (my_received_responses == my_expected_responses) {
            my_instruction->state = Instruction::Status::Finish;
            DPRINTF(MAAIndirect, "I[%d] %s: state set to finish for request %s!\n", my_indirect_id, __func__, my_instruction->print());
            state = Status::Idle;
            maa->spd->setReady(my_dst_tile);
            maa->finishInstruction(my_instruction, my_dst_tile);
            my_instruction = nullptr;
        } else {
            DPRINTF(MAAIndirect, "I[%d] %s: expected: %d, received: %d!\n", my_indirect_id, __func__, my_expected_responses, my_received_responses);
        }
        break;
    }
    default:
        assert(false);
    }
}
void IndirectAccessUnit::checkAllRowTablesSent() {
    assert(my_all_row_table_req_sent == false);
    my_all_row_table_req_sent = true;
    for (int i = 0; i < num_row_table_banks; i++) {
        if (my_row_table_req_sent[i] == false) {
            my_all_row_table_req_sent = false;
            return;
        }
    }
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
    DPRINTF(MAAIndirect, "I[%d] %s: Snoop of %s returned with isBlockCached(%s), satisfied (%s)\n",
            my_indirect_id,
            __func__,
            curr_pkt->print(),
            curr_pkt->isBlockCached() ? "true" : "false",
            curr_pkt->satisfied() ? "true" : "false");
    return curr_pkt->isBlockCached();
}
void IndirectAccessUnit::createMyPacket() {
    /**** Packet generation ****/
    RequestPtr real_req = std::make_shared<Request>(my_translated_block_physical_address,
                                                    block_size,
                                                    flags,
                                                    maa->requestorId);
    if (my_is_block_cached)
        my_pkt = new Packet(real_req, MemCmd::ReadSharedReq);
    else
        my_pkt = new Packet(real_req, MemCmd::ReadReq);
    my_pkt->allocate();
    my_outstanding_pkt = true;
    DPRINTF(MAAIndirect, "I[%d] %s: created %s\n", my_indirect_id, __func__, my_pkt->print());
}
bool IndirectAccessUnit::sendOutstandingPacket() {
    if (my_is_block_cached) {
        DPRINTF(MAAIndirect, "I[%d] %s: trying sending %s to cache\n", my_indirect_id, __func__, my_pkt->print());

        if (maa->cacheSidePort.sendPacket(FuncUnitType::INDIRECT,
                                          my_indirect_id,
                                          my_pkt) == false) {
            DPRINTF(MAAIndirect, "I[%d] %s: send failed, leaving execution...\n", my_indirect_id, __func__);
            return false;
        } else {
            my_outstanding_pkt = false;
            return true;
        }
    } else {
        DPRINTF(MAAIndirect, "I[%d] %s: trying sending %s to memory\n", my_indirect_id, __func__, my_pkt->print());
        if (maa->memSidePort.sendTimingReq(my_pkt) == false) {
            DPRINTF(MAAIndirect, "I[%d] %s: send failed, leaving execution...\n", my_indirect_id, __func__);
            return false;
        } else {
            my_outstanding_pkt = false;
            return true;
        }
    }
}
void IndirectAccessUnit::translatePacket() {
    /**** Address translation ****/
    RequestPtr translation_req = std::make_shared<Request>(my_virtual_addr,
                                                           block_size,
                                                           flags, maa->requestorId,
                                                           my_instruction->PC,
                                                           my_instruction->CID);
    ThreadContext *tc = maa->system->threads[my_instruction->CID];
    maa->mmu->translateTiming(translation_req, tc, this, BaseMMU::Read);
    // The above function immediately does the translation and calls the finish function
    assert(translation_done);
    translation_done = false;
}
bool IndirectAccessUnit::recvData(const Addr addr,
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
    assert(wids.size() == block_size / word_size);
    for (int i = 0; i < block_size / word_size; i++) {
        assert(wids[i] == i);
    }
    for (auto entry : entries) {
        int itr = entry.itr;
        int wid = entry.wid;
        DPRINTF(MAAIndirect, "I[%d] %s: itr (%d) wid (%d) matched!\n", my_indirect_id, __func__, itr, wid);
        maa->spd->setData(my_dst_tile, itr, data[wid]);
    }
    my_received_responses++;
    if (my_received_responses == my_expected_responses) {
        DPRINTF(MAAIndirect, "%s: all responses received, calling execution again in state %s!\n",
                __func__, status_names[(int)state]);
        scheduleExecuteInstructionEvent();
    } else {
        DPRINTF(MAAIndirect, "%s: expected: %d, received: %d responses!\n", __func__, my_expected_responses, my_received_responses);
    }
    return true;
}
void IndirectAccessUnit::finish(const Fault &fault,
                                const RequestPtr &req, ThreadContext *tc, BaseMMU::Mode mode) {
    assert(fault == NoFault);
    assert(translation_done == false);
    translation_done = true;
    my_translated_physical_address = req->getPaddr();
}
void IndirectAccessUnit::setInstruction(Instruction *_instruction) {
    assert(my_instruction == nullptr);
    my_instruction = _instruction;
}
void IndirectAccessUnit::scheduleExecuteInstructionEvent(int latency) {
    DPRINTF(MAAIndirect, "I[%d] %s: scheduling execute for the next %d cycles!\n", my_indirect_id, __func__, latency);
    Tick new_when = curTick() + latency;
    if (!executeInstructionEvent.scheduled()) {
        maa->schedule(executeInstructionEvent, new_when);
    } else {
        Tick old_when = executeInstructionEvent.when();
        if (new_when < old_when)
            maa->reschedule(executeInstructionEvent, new_when);
    }
}
} // namespace gem5