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
void RowTableEntry::allocate(int _num_row_table_entries_per_row,
                             OffsetTable *_offset_table,
                             IndirectAccessUnit *_indir_access) {
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
        if (entries_valid[i] == false && free_entry_id == -1) {
            free_entry_id = i;
        } else if (entries[i].addr == addr) {
            offset_table->insert(itr, wid, entries[i].last_itr);
            entries[i].last_itr = itr;
            return true;
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
void RowTable::allocate(int _num_row_table_rows,
                        int _num_row_table_entries_per_row,
                        OffsetTable *_offset_table,
                        IndirectAccessUnit *_indir_access) {
    offset_table = _offset_table;
    indir_access = _indir_access;
    num_row_table_rows = _num_row_table_rows;
    num_row_table_entries_per_row = _num_row_table_entries_per_row;
    entries = new RowTableEntry[num_row_table_rows];
    entries_valid = new bool[num_row_table_entries_per_row];
    entries_received = new bool[num_row_table_entries_per_row];
    last_sent_row_id = 0;
    for (int i = 0; i < num_row_table_rows; i++) {
        entries[i].allocate(num_row_table_entries_per_row,
                            offset_table,
                            indir_access);
        entries_valid[i] = false;
        entries_received[i] = false;
    }
}
void RowTable::insert(Addr Grow_addr,
                      Addr addr,
                      int itr,
                      int wid) {
    int free_row_id = -1;
    for (int i = 0; i < num_row_table_rows; i++) {
        if (entries_valid[i] == true && entries[i].Grow_addr == Grow_addr) {
            if (entries[i].insert(addr, itr, wid)) {
                DPRINTF(MAAIndirect, "%s: Grow[0x%lx] inserted in row[%d]!\n",
                        __func__,
                        Grow_addr,
                        i);
                return;
            }
        } else if (entries_valid[i] == false && free_row_id == -1) {
            free_row_id = i;
        }
    }
    if (free_row_id == -1) {
        printf("Error: no free row id found for addr(0x%lx) and Grow_addr(0x%lx)!\n",
               addr, Grow_addr);
        for (int i = 0; i < num_row_table_rows; i++) {
            printf("Row[i]: Grow_addr(0x%lx) Valid (%s), entries:", entries[i].Grow_addr, entries_valid[i] ? "T" : "F");
            for (int j = 0; j < num_row_table_entries_per_row; j++) {
                printf("  (%d: %s)", j, entries[i].entries_valid[j] ? "T" : "F");
            }
            printf("\n");
            assert(false);
        }
    }
    DPRINTF(MAAIndirect, "%s: Grow[0x%lx] adding to new row[%d]!\n",
            __func__,
            Grow_addr,
            free_row_id);
    entries[free_row_id].Grow_addr = Grow_addr;
    assert(entries[free_row_id].insert(addr, itr, wid) == true);
    entries_valid[free_row_id] = true;
}
void RowTable::reset() {
    for (int i = 0; i < num_row_table_rows; i++) {
        entries[i].reset();
        entries_valid[i] = false;
        entries_received[i] = false;
    }
    last_sent_row_id = 0;
}
bool RowTable::get_entry_send(Addr &addr,
                              bool &is_block_cached) {
    assert(last_sent_row_id <= num_row_table_rows);
    for (; last_sent_row_id < num_row_table_rows; last_sent_row_id++) {
        if (entries_valid[last_sent_row_id] &&
            entries[last_sent_row_id].get_entry_send(addr, is_block_cached)) {
            return true;
        }
    }
    return false;
}
std::vector<OffsetTableEntry> RowTable::get_entry_recv(Addr Grow_addr,
                                                       Addr addr,
                                                       bool is_block_cached) {
    std::vector<OffsetTableEntry> results;
    for (int i = 0; i < num_row_table_rows; i++) {
        if (entries_valid[i] == true && entries_received[i] == false && entries[i].Grow_addr == Grow_addr) {
            DPRINTF(MAAIndirect, "%s: Grow[0x%lx] hit with row[%d]!\n",
                    __func__,
                    Grow_addr,
                    i);
            std::vector<OffsetTableEntry> result = entries[i].get_entry_recv(addr, is_block_cached);
            results.insert(results.begin(), result.begin(), result.end());
            if (entries[i].all_entries_received()) {
                DPRINTF(MAAIndirect, "%s: all entries received!\n", __func__);
                entries_received[i] = false;
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
void IndirectAccessUnit::allocate(int _num_tile_elements,
                                  int _num_row_table_rows,
                                  int _num_row_table_entries_per_row,
                                  MAA *_maa) {
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
        row_table[i].allocate(num_row_table_rows,
                              num_row_table_entries_per_row,
                              offset_table,
                              this);
        my_row_table_req_sent[i] = false;
    }
    my_all_row_table_req_sent = false;
    translation_done = false;
    state = Status::Idle;
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
void IndirectAccessUnit::execute(Instruction *_instruction) {
    switch (state) {
    case Status::Idle: {
        assert(_instruction != nullptr);
        DPRINTF(MAAIndirect, "%s: idling %s!\n", __func__, _instruction->print());
        state = Status::Decode;
        [[fallthrough]];
    }
    case Status::Decode: {
        assert(_instruction != nullptr);
        DPRINTF(MAAIndirect, "%s: decoding %s!\n", __func__, _instruction->print());
        my_instruction = _instruction;

        // Decoding the instruction
        my_idx_tile = my_instruction->src1SpdID;
        my_dst_tile = my_instruction->dst1SpdID;
        my_cond_tile = my_instruction->condSpdID;
        my_max = maa->spd->getSize(my_idx_tile);

        // Initialization
        my_virtual_addr = 0;
        my_base_addr = my_instruction->baseAddr;
        my_outstanding_pkt = false;
        my_received_responses = 0;
        my_is_block_cached = false;
        my_last_row_table_sent = 0;
        for (int i = 0; i < num_row_table_banks; i++) {
            row_table[i].reset();
            my_row_table_req_sent[i] = false;
        }
        my_all_row_table_req_sent = false;

        // Reordering the indices
        for (int i = 0; i < my_max; i++) {
            if (my_cond_tile == -1 || maa->spd->getData(my_cond_tile, i) != 0) {
                uint32_t idx = maa->spd->getData(my_idx_tile, i);
                Addr curr_addr = my_base_addr + word_size * idx;
                my_virtual_addr = addrBlockAlign(curr_addr, block_size);
                translatePacket();
                my_translated_block_physical_address = addrBlockAlign(my_translated_physical_address, block_size);
                uint16_t wid = (curr_addr - my_virtual_addr) / word_size;
                std::vector addr_vec = maa->map_addr(my_translated_block_physical_address);
                Addr Grow_addr = maa->calc_Grow_addr(addr_vec);
                int row_table_idx = getRowTableIdx(addr_vec[ADDR_CHANNEL_LEVEL],
                                                   addr_vec[ADDR_RANK_LEVEL],
                                                   addr_vec[ADDR_BANKGROUP_LEVEL]);
                row_table[row_table_idx].insert(Grow_addr,
                                                my_translated_block_physical_address,
                                                i,
                                                wid);
                DPRINTF(MAAIndirect, "%s: addr(0x%lx), blockaddr(0x%lx), Grow(0x%lx), itr(%d), wid(%d) inserted to table(%d)\n",
                        __func__,
                        my_translated_physical_address,
                        my_translated_block_physical_address,
                        Grow_addr,
                        i,
                        wid,
                        row_table_idx);
            }
        }
        maa->spd->setSize(my_dst_tile, my_max);

        // Setting the state of the instruction and stream unit
        my_instruction->state = Instruction::Status::Service;
        DPRINTF(MAAIndirect, "%s: state set to Request for request %s!\n", __func__, my_instruction->print());
        state = Status::Request;
        [[fallthrough]];
    }
    case Status::Request: {
        DPRINTF(MAAIndirect, "%s: requesting %s!\n", __func__, my_instruction->print());
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
                DPRINTF(MAAIndirect, "%s: Checking row table bank[%d]!\n", __func__, row_table_idx);
                if (my_row_table_req_sent[row_table_idx] == false) {
                    if (row_table[row_table_idx].get_entry_send(my_translated_block_physical_address, my_is_block_cached)) {
                        DPRINTF(MAAIndirect, "%s: Creating packet for bank IDX[%d]!\n", __func__, row_table_idx);
                        createMyPacket();
                        if (sendOutstandingPacket() == false) {
                            my_last_row_table_sent++;
                            break;
                        }
                    } else {
                        DPRINTF(MAAIndirect, "%s: Row table bank[%d] has nothing, setting sent to true!\n", __func__, row_table_idx);
                        my_row_table_req_sent[row_table_idx] = true;
                    }
                } else {
                    DPRINTF(MAAIndirect, "%s: Row table bank[%d] has already sent the requests!\n", __func__, row_table_idx);
                }
            }
            my_last_row_table_sent = (my_last_row_table_sent >= num_row_table_banks) ? 0 : my_last_row_table_sent;
        }
        if (my_outstanding_pkt) {
            break;
        }
        DPRINTF(MAAIndirect, "%s: state set to respond for request %s!\n", __func__, my_instruction->print());
        state = Status::Response;
        [[fallthrough]];
    }
    case Status::Response: {
        DPRINTF(MAAIndirect, "%s: responding %s!\n", __func__, my_instruction->print());
        if (my_received_responses == my_max) {
            my_instruction->state = Instruction::Status::Finish;
            DPRINTF(MAAIndirect, "%s: state set to finish for request %s!\n", __func__, my_instruction->print());
            state = Status::Idle;
            maa->spd->setReady(my_dst_tile);
            maa->finishInstruction(my_instruction, my_dst_tile);
        } else {
            DPRINTF(MAAIndirect, "%s: expected: %d, received: %d!\n", __func__, my_max, my_received_responses);
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
    DPRINTF(MAAIndirect, "%s: sending snoop of %s\n", __func__, curr_pkt->print());
    maa->cpuSidePort.sendTimingSnoopReq(curr_pkt);
    // assert(curr_pkt->satisfied() == false);
    DPRINTF(MAAIndirect, "%s: Snoop of %s returned with isBlockCached(%s), satisfied (%s)\n",
            __func__, curr_pkt->print(),
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
    DPRINTF(MAAIndirect, "%s: created %s\n", __func__, my_pkt->print());
}
bool IndirectAccessUnit::sendOutstandingPacket() {
    if (my_is_block_cached) {
        DPRINTF(MAAIndirect, "%s: trying sending %s to cache\n", __func__, my_pkt->print());
        if (maa->cacheSidePort.sendTimingReq(my_pkt) == false) {
            DPRINTF(MAAIndirect, "%s: send failed, leaving execution...\n", __func__);
            return false;
        } else {
            my_outstanding_pkt = false;
            return true;
        }
    } else {
        DPRINTF(MAAIndirect, "%s: trying sending %s to memory\n", __func__, my_pkt->print());
        if (maa->memSidePort.sendTimingReq(my_pkt) == false) {
            DPRINTF(MAAIndirect, "%s: send failed, leaving execution...\n", __func__);
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
void IndirectAccessUnit::recvData(const Addr addr,
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
    DPRINTF(MAAIndirect, "%s: %d entries received for addr(0x%lx), Grow(x%lx) from table(%d)!\n",
            __func__,
            entries.size(),
            addr,
            Grow_addr,
            row_table_idx);
    assert(wids.size() == block_size / word_size);
    for (int i = 0; i < block_size / word_size; i++) {
        assert(wids[i] == i);
    }
    for (auto entry : entries) {
        int itr = entry.itr;
        int wid = entry.wid;
        DPRINTF(MAAIndirect, "%s: itr (%d) wid (%d) matched!\n", __func__, itr, wid);
        maa->spd->setData(my_dst_tile, itr, data[wid]);
        my_received_responses++;
    }
    if (state == Status::Response) {
        execute(my_instruction);
    }
}
void IndirectAccessUnit::finish(const Fault &fault,
                                const RequestPtr &req, ThreadContext *tc, BaseMMU::Mode mode) {
    assert(fault == NoFault);
    assert(translation_done == false);
    translation_done = true;
    my_translated_physical_address = req->getPaddr();
}
} // namespace gem5