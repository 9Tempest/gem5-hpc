#include "mem/MAA/IndirectAccess.hh"
#include "base/logging.hh"
#include "mem/MAA/MAA.hh"
#include "mem/MAA/SPD.hh"
#include "mem/MAA/IF.hh"
#include "base/trace.hh"
#include "base/types.hh"
#include "debug/MAAIndirect.hh"
#include "debug/MAATrace.hh"
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
                             int _num_RT_entries_per_row,
                             OffsetTable *_offset_table,
                             IndirectAccessUnit *_indir_access) {
    my_indirect_id = _my_indirect_id;
    my_table_id = _my_table_id;
    my_table_row_id = _my_table_row_id;
    offset_table = _offset_table;
    indir_access = _indir_access;
    num_RT_entries_per_row = _num_RT_entries_per_row;
    entries = new Entry[num_RT_entries_per_row];
    entries_valid = new bool[num_RT_entries_per_row];
    last_sent_entry_id = 0;
    for (int i = 0; i < num_RT_entries_per_row; i++) {
        entries_valid[i] = false;
    }
}
bool RowTableEntry::find_addr(Addr addr) {
    for (int i = 0; i < num_RT_entries_per_row; i++) {
        if (entries_valid[i] == true && entries[i].addr == addr) {
            return true;
        }
    }
    return false;
}
bool RowTableEntry::insert(Addr addr, int itr, int wid) {
    int free_entry_id = -1;
    for (int i = 0; i < num_RT_entries_per_row; i++) {
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
    for (int i = 0; i < num_RT_entries_per_row; i++) {
        panic_if(entries_valid[i], "Entry %d is valid: addr(0x%lx)!\n", i, entries[i].addr);
    }
    panic_if(last_sent_entry_id != 0, "Last sent entry id is not 0: %d!\n", last_sent_entry_id);
}
void RowTableEntry::reset() {
    for (int i = 0; i < num_RT_entries_per_row; i++) {
        entries_valid[i] = false;
    }
    last_sent_entry_id = 0;
}
bool RowTableEntry::get_entry_send(Addr &addr) {
    assert(last_sent_entry_id <= num_RT_entries_per_row);
    for (; last_sent_entry_id < num_RT_entries_per_row; last_sent_entry_id++) {
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
    for (int i = 0; i < num_RT_entries_per_row; i++) {
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
    for (int i = 0; i < num_RT_entries_per_row; i++) {
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
                        int _num_RT_rows_per_bank,
                        int _num_RT_entries_per_row,
                        OffsetTable *_offset_table,
                        IndirectAccessUnit *_indir_access) {
    my_indirect_id = _my_indirect_id;
    my_table_id = _my_table_id;
    offset_table = _offset_table;
    indir_access = _indir_access;
    num_RT_rows_per_bank = _num_RT_rows_per_bank;
    num_RT_entries_per_row = _num_RT_entries_per_row;
    entries = new RowTableEntry[num_RT_rows_per_bank];
    entries_valid = new bool[num_RT_rows_per_bank];
    entries_sent = new bool[num_RT_rows_per_bank];
    // entries_full = new bool[num_RT_rows_per_bank];
    last_sent_grow_addr = 0;
    last_sent_rowid = 0;
    last_sent_grow_rowid = 0;
    for (int i = 0; i < num_RT_rows_per_bank; i++) {
        entries[i].allocate(my_indirect_id,
                            my_table_id,
                            i,
                            num_RT_entries_per_row,
                            offset_table,
                            indir_access);
        entries_valid[i] = false;
        entries_sent[i] = false;
        // entries_full[i] = false;
    }
}
bool RowTable::insert(Addr grow_addr, Addr addr, int itr, int wid) {
    // 1. Check if the (Row, CL) pair exists
    for (int i = 0; i < num_RT_rows_per_bank; i++) {
        if (entries_valid[i] == true && entries[i].grow_addr == grow_addr) {
            if (entries[i].find_addr(addr)) {
                DPRINTF(MAAIndirect, "I[%d] T[%d] %s: grow[0x%lx] addr[0x%lx] found in R[%d]!\n",
                        my_indirect_id, my_table_id, __func__, grow_addr, addr, i);
                assert(entries[i].insert(addr, itr, wid));
                return true;
            }
        }
    }
    // 2. Check if (Row) exists and can insert the new CL
    // 2.1 At the same time, look for a free row
    int free_row_id = -1;
    int num_free_entries = 0;
    for (int i = 0; i < num_RT_rows_per_bank; i++) {
        if (entries_valid[i] == true && entries[i].grow_addr == grow_addr) {
            if (entries[i].insert(addr, itr, wid)) {
                DPRINTF(MAAIndirect, "I[%d] T[%d] %s: grow[0x%lx] R[%d] inserted new addr[0x%lx]!\n",
                        my_indirect_id, my_table_id, __func__, grow_addr, i, addr);
                return true;
            }
        } else if (entries_valid[i] == false) {
            num_free_entries++;
            if (free_row_id == -1) {
                free_row_id = i;
            }
        }
    }
    // 3. Check if we can insert the new Row or we need drain
    if (free_row_id == -1) {
        DPRINTF(MAAIndirect, "I[%d] T[%d] %s: no entry exists or available for grow[0x%lx] and addr[0x%lx], requires drain. Avg CL/Row: %d!\n",
                my_indirect_id, my_table_id, __func__, grow_addr, addr, getAverageEntriesPerRow());
        return false;
    }
    // 4. Add new (Row), add new (CL)
    DPRINTF(MAAIndirect, "I[%d] T[%d] %s: grow[0x%lx] adding to new R[%d]!\n", my_indirect_id, my_table_id, __func__, grow_addr, free_row_id);
    entries[free_row_id].grow_addr = grow_addr;
    assert(entries[free_row_id].insert(addr, itr, wid) == true);
    entries_valid[free_row_id] = true;
    entries_sent[free_row_id] = false;
    if (num_free_entries == 1) {
        DPRINTF(MAAIndirect, "I[%d] T[%d] %s: R[%d] grow[0x%lx] set to full!\n", my_indirect_id, my_table_id, __func__, free_row_id, grow_addr);
        // entries_full[free_row_id] = true;
    }
    (*indir_access->maa->stats.IND_NumRowsInserted[my_indirect_id])++;
    return true;
}
float RowTable::getAverageEntriesPerRow() {
    float total_entries = 0.0000;
    for (int i = 0; i < num_RT_rows_per_bank; i++) {
        if (entries_valid[i] == true) {
            for (int j = 0; j < num_RT_entries_per_row; j++) {
                total_entries += entries[i].entries_valid[j] ? 1 : 0;
            }
        }
    }
    return total_entries / num_RT_rows_per_bank;
}
void RowTable::check_reset() {
    for (int i = 0; i < num_RT_rows_per_bank; i++) {
        entries[i].check_reset();
        panic_if(entries_valid[i], "Row[%d] is valid: grow_addr(0x%lx)!\n", i, entries[i].grow_addr);
        // panic_if(entries_full[i], "Row[%d] is full: grow_addr(0x%lx)!\n", i, entries[i].grow_addr);
    }
    // panic_if(last_sent_rowid != 0, "Last sent row id is not 0: %d!\n", last_sent_rowid);
}
void RowTable::reset() {
    for (int i = 0; i < num_RT_rows_per_bank; i++) {
        entries[i].reset();
        entries_valid[i] = false;
        entries_sent[i] = false;
        // entries_full[i] = false;
    }
    last_sent_rowid = 0;
    last_sent_grow_rowid = 0;
    last_sent_grow_addr = 0;
}
bool RowTable::find_next_grow_addr() {
    for (; last_sent_rowid < num_RT_rows_per_bank; last_sent_rowid++) {
        if (entries_valid[last_sent_rowid] == true && entries_sent[last_sent_rowid] == false) {
            last_sent_grow_rowid = last_sent_rowid;
            last_sent_grow_addr = entries[last_sent_rowid].grow_addr;
            return true;
        }
    }
    last_sent_rowid = 0;
    return false;
}
void RowTable::get_send_grow_rowid() {
    for (; last_sent_grow_rowid < num_RT_rows_per_bank; last_sent_grow_rowid++) {
        if (entries_valid[last_sent_grow_rowid] == true && entries_sent[last_sent_grow_rowid] == false && entries[last_sent_grow_rowid].grow_addr == last_sent_grow_addr) {
            return;
        }
    }
    last_sent_grow_rowid = 0;
    last_sent_grow_addr = 0;
}
bool RowTable::get_entry_send(Addr &addr, bool drain) {
    while (true) {
        if (last_sent_grow_addr == 0) {
            if (find_next_grow_addr() == false)
                return false;
        }
        panic_if(entries_valid[last_sent_grow_rowid] == false, "Row[%d] is invalid: grow_addr(0x%lx)!\n", last_sent_grow_rowid, last_sent_grow_addr);
        panic_if(entries_sent[last_sent_grow_rowid] == true, "Row[%d] is already sent: grow_addr(0x%lx)!\n", last_sent_grow_rowid, last_sent_grow_addr);
        if (entries[last_sent_grow_rowid].get_entry_send(addr)) {
            DPRINTF(MAAIndirect, "I[%d] T[%d] %s: R[%d] retuned!\n", my_indirect_id, my_table_id, __func__, last_sent_grow_rowid);
            return true;
        } else {
            DPRINTF(MAAIndirect, "I[%d] T[%d] %s: R[%d] finished!\n", my_indirect_id, my_table_id, __func__, last_sent_grow_rowid);
        }
        entries_sent[last_sent_grow_rowid] = true;
        get_send_grow_rowid();
    }
    return false;
}
std::vector<OffsetTableEntry> RowTable::get_entry_recv(Addr grow_addr, Addr addr) {
    std::vector<OffsetTableEntry> results;
    for (int i = 0; i < num_RT_rows_per_bank; i++) {
        if (entries_valid[i] == true && entries[i].grow_addr == grow_addr) {
            std::vector<OffsetTableEntry> result = entries[i].get_entry_recv(addr);
            if (result.size() == 0) {
                DPRINTF(MAAIndirect, "I[%d] T[%d] %s: grow[0x%lx] addr[0x%lx] hit with R[%d] but no CLs returned!\n", my_indirect_id, my_table_id, __func__, grow_addr, addr, i);
                continue;
            }
            DPRINTF(MAAIndirect, "I[%d] T[%d] %s: grow[0x%lx] addr[0x%lx] hit with R[%d], %d entries returned!\n", my_indirect_id, my_table_id, __func__, grow_addr, addr, i, result.size());
            panic_if(results.size() != 0, "I[%d] T[%d] %s: duplicate entry is not allowed!\n", my_indirect_id, my_table_id, __func__);
            results.insert(results.begin(), result.begin(), result.end());
            if (entries[i].all_entries_received()) {
                DPRINTF(MAAIndirect, "I[%d] T[%d] %s: all R[%d] entries received, setting to invalid!\n", my_indirect_id, my_table_id, __func__, i);
                entries_valid[i] = false;
                entries_sent[i] = false;
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
      sendCachePacketEvent([this] { sendOutstandingCachePacket(); }, name()),
      sendCpuPacketEvent([this] { sendOutstandingCpuPacket(); }, name()),
      sendMemReadPacketEvent([this] { sendOutstandingMemReadPacket(); }, name()),
      sendMemWritePacketEvent([this] { sendOutstandingMemWritePacket(); }, name()) {
    RT_bank_org = nullptr;
    num_RT_banks = nullptr;
    num_RT_rows_total = nullptr;
    num_RT_possible_grows = nullptr;
    num_RT_subbanks = nullptr;
    num_RT_bank_columns = nullptr;
    RT_config_addr = nullptr;
    RT_config_cache = nullptr;
    RT_config_cache_tick = nullptr;
    RT = nullptr;
    offset_table = nullptr;
    my_RT_req_sent = nullptr;
    my_RT_bank_order = nullptr;
    my_instruction = nullptr;
}
IndirectAccessUnit::~IndirectAccessUnit() {
    assert(RT_bank_org != nullptr);
    for (int i = 0; i < num_RT_configs; i++) {
        assert(RT_bank_org[i] != nullptr);
        delete[] RT_bank_org[i];
    }
    delete[] RT_bank_org;
    assert(num_RT_banks != nullptr);
    delete[] num_RT_banks;
    assert(num_RT_rows_total != nullptr);
    delete[] num_RT_rows_total;
    assert(num_RT_possible_grows != nullptr);
    delete[] num_RT_possible_grows;
    assert(num_RT_subbanks != nullptr);
    delete[] num_RT_subbanks;
    assert(num_RT_bank_columns != nullptr);
    delete[] num_RT_bank_columns;
    assert(RT_config_addr != nullptr);
    delete[] RT_config_addr;
    assert(RT_config_cache != nullptr);
    delete[] RT_config_cache;
    assert(RT_config_cache_tick != nullptr);
    delete[] RT_config_cache_tick;
    assert(RT != nullptr);
    for (int i = 0; i < num_RT_configs; i++) {
        assert(RT[i] != nullptr);
        delete[] RT[i];
    }
    delete[] RT;
    assert(offset_table != nullptr);
    delete offset_table;
    assert(my_RT_req_sent != nullptr);
    for (int i = 0; i < num_RT_configs; i++) {
        assert(my_RT_req_sent[i] != nullptr);
        delete[] my_RT_req_sent[i];
    }
    delete[] my_RT_req_sent;
    assert(my_RT_bank_order != nullptr);
    delete[] my_RT_bank_order;
}
void IndirectAccessUnit::allocate(int _my_indirect_id,
                                  int _num_tile_elements,
                                  int _num_row_table_rows_per_bank,
                                  int _num_row_table_entries_per_subbank_row,
                                  int _num_row_table_config_cache_entries,
                                  bool _reconfigure_row_table,
                                  int _num_initial_row_table_banks,
                                  Cycles _rowtable_latency,
                                  Cycles _cache_snoop_latency,
                                  int _num_channels,
                                  int _num_cores,
                                  MAA *_maa) {
    my_indirect_id = _my_indirect_id;
    maa = _maa;
    num_tile_elements = _num_tile_elements;
    num_RT_rows_per_bank = _num_row_table_rows_per_bank;
    num_RT_entries_per_subbank_row = _num_row_table_entries_per_subbank_row;
    num_RT_config_cache_entries = _num_row_table_config_cache_entries;
    reconfigure_RT = _reconfigure_row_table;
    num_initial_RT_banks = _num_initial_row_table_banks;
    rowtable_latency = _rowtable_latency;
    cache_snoop_latency = _cache_snoop_latency;
    num_channels = _num_channels;
    num_cores = _num_cores;
    my_translation_done = false;
    state = Status::Idle;
    my_instruction = nullptr;
    dst_tile_id = -1;

    mem_channels_blocked = new bool[num_channels];
    for (int i = 0; i < num_channels; i++) {
        mem_channels_blocked[i] = false;
    }

    // core_blocked = new bool[num_cores];
    // for (int i = 0; i < num_cores; i++) {
    //     core_blocked[i] = false;
    // }

    offset_table = new OffsetTable();
    offset_table->allocate(my_indirect_id, num_tile_elements, this);

    // Row Table initialization
    int min_num_RT_banks = maa->m_org[ADDR_CHANNEL_LEVEL] * maa->m_org[ADDR_RANK_LEVEL] * 2;
    Addr max_num_RT_possible_grows = 2 * maa->m_org[ADDR_BANK_LEVEL] * maa->m_org[ADDR_ROW_LEVEL];
    total_num_RT_subbanks = maa->m_org[ADDR_CHANNEL_LEVEL] * maa->m_org[ADDR_RANK_LEVEL] *
                            maa->m_org[ADDR_BANKGROUP_LEVEL] * maa->m_org[ADDR_BANK_LEVEL];
    num_RT_configs = log2((double)total_num_RT_subbanks / (double)min_num_RT_banks) + 1;

    RT_config_addr = new Addr[num_RT_config_cache_entries];
    RT_config_cache = new int[num_RT_config_cache_entries];
    RT_config_cache_tick = new Tick[num_RT_config_cache_entries];
    for (int i = 0; i < num_RT_config_cache_entries; i++) {
        RT_config_addr[i] = 0;
        RT_config_cache[i] = -1;
        RT_config_cache_tick[i] = 0;
    }

    RT = new RowTable *[num_RT_configs];
    my_RT_req_sent = new bool *[num_RT_configs];
    my_RT_bank_order = new std::vector<int>[num_RT_configs];
    RT_bank_org = new int *[num_RT_configs];
    num_RT_banks = new int[num_RT_configs];
    num_RT_rows_total = new int[num_RT_configs];
    num_RT_subbanks = new int[num_RT_configs];
    num_RT_bank_columns = new int[num_RT_configs];
    num_RT_possible_grows = new Addr[num_RT_configs];

    int current_num_RT_banks = min_num_RT_banks;
    int current_num_RT_rows_total = current_num_RT_banks * num_RT_rows_per_bank;
    Addr current_num_RT_possible_grows = max_num_RT_possible_grows;
    int current_num_RT_subbanks = total_num_RT_subbanks / min_num_RT_banks;
    int current_num_RT_entries_per_row = num_RT_entries_per_subbank_row * current_num_RT_subbanks;
    initial_RT_config = -1;
    for (int i = 0; i < num_RT_configs; i++) {
        RT[i] = new RowTable[current_num_RT_banks];
        my_RT_req_sent[i] = new bool[current_num_RT_banks];
        num_RT_banks[i] = current_num_RT_banks;
        num_RT_rows_total[i] = current_num_RT_rows_total;
        num_RT_subbanks[i] = current_num_RT_subbanks;
        num_RT_bank_columns[i] = current_num_RT_entries_per_row;
        num_RT_possible_grows[i] = current_num_RT_possible_grows;
        if (reconfigure_RT == false && current_num_RT_banks == num_initial_RT_banks) {
            initial_RT_config = i;
        }
        panic_if(current_num_RT_entries_per_row <= 0, "I[%d] TC[%d] %s: current_num_RT_entries_per_row is %d!\n",
                 my_indirect_id, i, __func__, current_num_RT_entries_per_row);
        for (int j = 0; j < current_num_RT_banks; j++) {
            RT[i][j].allocate(my_indirect_id, j, num_RT_rows_per_bank, current_num_RT_entries_per_row, offset_table, this);
            my_RT_req_sent[i][j] = false;
        }

        // How many banks corresponding to which level exist in
        // this configuration (RowTable Bank Organization)
        RT_bank_org[i] = new int[ADDR_MAX_LEVEL];
        int remaining_banks = current_num_RT_banks;
        for (int k = 0; k < ADDR_MAX_LEVEL; k++) {
            if (remaining_banks > maa->m_org[k]) {
                RT_bank_org[i][k] = maa->m_org[k];
                assert(remaining_banks % maa->m_org[k] == 0);
                remaining_banks /= maa->m_org[k];
            } else if (remaining_banks > 0) {
                RT_bank_org[i][k] = remaining_banks;
                remaining_banks = 0;
            } else {
                RT_bank_org[i][k] = 1;
            }
        }
        DPRINTF(MAAIndirect, "I[%d] TC[%d]: %d banks x %d subbanks x %d rows x %d columns -- CH: %d, RA: %d, BG: %d, BA: %d, RO: %d, CO: %d\n",
                my_indirect_id, i, num_RT_banks[i], num_RT_subbanks[i], num_RT_rows_per_bank, num_RT_bank_columns[i],
                RT_bank_org[i][ADDR_CHANNEL_LEVEL], RT_bank_org[i][ADDR_RANK_LEVEL],
                RT_bank_org[i][ADDR_BANKGROUP_LEVEL], RT_bank_org[i][ADDR_BANK_LEVEL],
                RT_bank_org[i][ADDR_ROW_LEVEL], RT_bank_org[i][ADDR_COLUMN_LEVEL]);

        my_RT_bank_order[i].clear();
        for (int bank = 0; bank < maa->m_org[ADDR_BANK_LEVEL]; bank++) {
            for (int bankgroup = 0; bankgroup < maa->m_org[ADDR_BANKGROUP_LEVEL]; bankgroup++) {
                for (int rank = 0; rank < maa->m_org[ADDR_RANK_LEVEL]; rank++) {
                    for (int channel = 0; channel < maa->m_org[ADDR_CHANNEL_LEVEL]; channel++) {
                        int RT_index = getRowTableIdx(i, channel, rank, bankgroup, bank);
                        if (std::find(my_RT_bank_order[i].begin(),
                                      my_RT_bank_order[i].end(),
                                      RT_index) == my_RT_bank_order[i].end()) {
                            my_RT_bank_order[i].push_back(RT_index);
                        }
                    }
                }
            }
        }
        panic_if(my_RT_bank_order[i].size() != num_RT_banks[i],
                 "I[%d] TC[%d] %s: my_RT_bank_order(%d) != num_RT_banks(%d)!\n",
                 my_indirect_id, i, __func__, my_RT_bank_order[i].size(), num_RT_banks[i]);
        current_num_RT_banks *= 2;
        current_num_RT_rows_total *= 2;
        current_num_RT_subbanks /= 2;
        current_num_RT_entries_per_row /= 2;
        current_num_RT_possible_grows /= 2;
    }
    if (reconfigure_RT)
        initial_RT_config = num_RT_configs - 1;
    DPRINTF(MAAIndirect, "I[%d] %s: initial_RT_config(%d)!\n", my_indirect_id, __func__, initial_RT_config);
}
int IndirectAccessUnit::getRowTableIdx(int RT_config, int channel, int rank, int bankgroup, int bank) {
    int RT_index = 0;
    RT_index += (channel % RT_bank_org[RT_config][ADDR_CHANNEL_LEVEL]);
    RT_index *= (RT_bank_org[RT_config][ADDR_RANK_LEVEL]);
    RT_index += (rank % RT_bank_org[RT_config][ADDR_RANK_LEVEL]);
    RT_index *= (RT_bank_org[RT_config][ADDR_BANKGROUP_LEVEL]);
    RT_index += (bankgroup % RT_bank_org[RT_config][ADDR_BANKGROUP_LEVEL]);
    RT_index *= (RT_bank_org[RT_config][ADDR_BANK_LEVEL]);
    RT_index += (bank % RT_bank_org[RT_config][ADDR_BANK_LEVEL]);
    panic_if(RT_index >= num_RT_banks[RT_config],
             "I[%d] TC[%d] %s: RT_index(%d) >= num_RT_banks(%d)!\n",
             my_indirect_id, RT_config, __func__, RT_index, num_RT_banks[RT_config]);
    return RT_index;
}
Addr IndirectAccessUnit::getGrowAddr(int RT_config, int bankgroup, int bank, int row) {
    Addr grow_addr = 0;
    grow_addr = (bankgroup / RT_bank_org[RT_config][ADDR_BANKGROUP_LEVEL]);
    grow_addr *= maa->m_org[ADDR_BANK_LEVEL];
    grow_addr += (bank / RT_bank_org[RT_config][ADDR_BANK_LEVEL]);
    grow_addr *= maa->m_org[ADDR_ROW_LEVEL];
    grow_addr += (row / RT_bank_org[RT_config][ADDR_ROW_LEVEL]);
    assert(RT_bank_org[RT_config][ADDR_ROW_LEVEL] == 1);
    panic_if(grow_addr >= num_RT_possible_grows[RT_config],
             "I[%d] TC[%d] %s: grow_addr(%lu) >= num_RT_possible_grows(%lu)!\n",
             my_indirect_id, RT_config, __func__, grow_addr, num_RT_possible_grows[RT_config]);
    return grow_addr;
}
int IndirectAccessUnit::getRowTableConfig(Addr addr) {
    if (reconfigure_RT == false)
        return initial_RT_config;

    int oldest_entry = -1;
    Tick oldest_tick = 0;
    Tick current_tick = curTick();
    for (int i = 0; i < num_RT_config_cache_entries; i++) {
        if (RT_config_addr[i] == addr) {
            RT_config_cache_tick[i] = current_tick;
            return RT_config_cache[i];
        } else if (RT_config_cache_tick[i] <= oldest_tick) {
            oldest_tick = RT_config_cache_tick[i];
            oldest_entry = i;
        }
    }
    assert(oldest_entry != -1);
    RT_config_addr[oldest_entry] = addr;
    RT_config_cache[oldest_entry] = initial_RT_config;
    RT_config_cache_tick[oldest_entry] = current_tick;
    return initial_RT_config;
}
void IndirectAccessUnit::setRowTableConfig(Addr addr, int num_CLs, int num_ROWs) {
    if (reconfigure_RT == false)
        return;

    // This approach selects the configuration with as many ROWs as needed
    int new_config = -1;
    if (num_ROWs >= num_RT_rows_total[num_RT_configs - 1]) {
        new_config = num_RT_configs - 1;
    } else {
        for (int i = 0; i < num_RT_configs; i++) {
            if (num_ROWs < num_RT_rows_total[i]) {
                new_config = i;
                break;
            }
        }
    }

#if 0
    // This approach selects the configuration with as many ROWs as needed
    int new_config = -1;
    if (num_ROWs >= num_RT_rows_total[num_RT_configs - 1]) {
        new_config = num_RT_configs - 1;
    } else {
        for (int i = 0; i < num_RT_configs - 1; i++) {
            if (num_ROWs < ((num_RT_rows_total[i] + num_RT_rows_total[i + 1]) / 2)) {
                new_config = i;
                break;
            }
        }
    }
#endif

#if 0
    // This approach does not work. If D is too large, there will be many unique CLs
    // and many unique ROWs. The CL/ROW is still large, but since num_ROWs >> number
    // of RT banks x RT subbanks x rows/subbank, there will be a lot of drains.
    int num_CLs_per_ROW = num_CLs / num_ROWs;
    if (num_CLs_per_ROW >= num_RT_bank_columns[0]) {
        new_config = 0;
    } else if (num_CLs_per_ROW < num_RT_bank_columns[num_RT_configs - 1]) {
        new_config = num_RT_configs - 1;
    } else {
        for (int i = 1; i < num_RT_configs; i++) {
            if (num_CLs_per_ROW < num_RT_bank_columns[i - 1] &&
                num_CLs_per_ROW >= num_RT_bank_columns[i]) {
                new_config = i;
            }
        }
    }
#endif

    assert(new_config != -1);
    for (int i = 0; i < num_RT_config_cache_entries; i++) {
        if (RT_config_addr[i] == addr) {
            RT_config_cache[i] = new_config;
            DPRINTF(MAATrace, "I[%d] %s: addr(0x%lx) set to config(%d) with (%d/%d) CLs, (%d/%d) ROWs, (%d/%d) CLs/ROW!\n",
                    my_indirect_id, __func__, addr, new_config,
                    num_CLs, num_RT_bank_columns[new_config] * num_RT_banks[new_config] * num_RT_rows_per_bank,
                    num_ROWs, num_RT_rows_total[new_config],
                    num_CLs / num_ROWs, num_RT_bank_columns[new_config]);
            return;
        }
    }
    panic_if(true, "I[%d] %s: addr(0x%lx) not found in the cache!\n", my_indirect_id, __func__, addr);
}
void IndirectAccessUnit::check_reset() {
    for (int i = 0; i < num_RT_configs; i++) {
        for (int j = 0; j < num_RT_banks[i]; j++) {
            RT[i][j].check_reset();
        }
    }
    offset_table->check_reset();
    panic_if(my_outstanding_cache_read_pkts.size() != 0, "Outstanding cache read packets: %d!\n", my_outstanding_cache_read_pkts.size());
    // panic_if(my_outstanding_cache_evict_pkts.size() != 0, "Outstanding cache evict packets: %d!\n", my_outstanding_cache_evict_pkts.size());
    panic_if(my_outstanding_cpu_snoop_pkts.size() != 0, "Outstanding cache snoop packets: %d!\n", my_outstanding_cpu_snoop_pkts.size());
    panic_if(my_outstanding_mem_read_pkts.size() != 0, "Outstanding mem read packets: %d!\n", my_outstanding_mem_read_pkts.size());
    panic_if(my_outstanding_mem_write_pkts.size() != 0, "Outstanding mem write packets: %d!\n", my_outstanding_mem_write_pkts.size());
    panic_if(my_decode_start_tick != 0, "Decode start tick is not 0: %lu!\n", my_decode_start_tick);
    panic_if(my_fill_start_tick != 0, "Fill start tick is not 0: %lu!\n", my_fill_start_tick);
    panic_if(my_build_start_tick != 0, "Build start tick is not 0: %lu!\n", my_build_start_tick);
    panic_if(my_request_start_tick != 0, "Request start tick is not 0: %lu!\n", my_request_start_tick);
}
Cycles IndirectAccessUnit::updateLatency(int num_spd_read_data_accesses, int num_spd_read_condidx_accesses, int num_spd_write_accesses, int num_rowtable_accesses, int RT_access_parallelism) {
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
        num_rowtable_accesses = getCeiling(num_rowtable_accesses, RT_access_parallelism);
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
bool IndirectAccessUnit::scheduleNextSendCpu() {
    if (my_outstanding_cpu_snoop_pkts.size() > 0) {
        Cycles latency = Cycles(0);
        if (my_outstanding_cpu_snoop_pkts.begin()->tick > curTick()) {
            latency = maa->getTicksToCycles(my_outstanding_cpu_snoop_pkts.begin()->tick - curTick());
        }
        scheduleSendCpuPacketEvent(latency);
        return true;
    }
    return false;
}
bool IndirectAccessUnit::scheduleNextSendCache() {
    if (my_outstanding_cache_read_pkts.size() > 0) { //  || my_outstanding_cache_evict_pkts.size() > 0
        scheduleSendCachePacketEvent(Cycles(0));
        return true;
    }
    return false;
}
bool IndirectAccessUnit::scheduleNextSendMemRead() {
    if (my_outstanding_mem_read_pkts.size() > 0) {
        scheduleSendMemReadPacketEvent(Cycles(0));
        return true;
    }
    return false;
}
bool IndirectAccessUnit::scheduleNextSendMemWrite() {
    bool return_val = false;
    for (auto it = my_outstanding_mem_write_pkts.begin(); it != my_outstanding_mem_write_pkts.end();) {
        if (mem_channels_blocked[maa->channel_addr(it->packet->getAddr())]) {
            return_val = true;
            ++it;
            continue;
        }
        Cycles latency = Cycles(0);
        if (it->tick > curTick()) {
            latency = maa->getTicksToCycles(it->tick - curTick());
        }
        scheduleSendMemWritePacketEvent(latency);
        return_val = true;
        break;
    }
    return return_val;
}
void IndirectAccessUnit::executeInstruction() {
    switch (state) {
    case Status::Idle: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: idling %s!\n", my_indirect_id, __func__, my_instruction->print());
        DPRINTF(MAATrace, "I[%d] Start [%s]\n", my_indirect_id, my_instruction->print());
        state = Status::Decode;
        [[fallthrough]];
    }
    case Status::Decode: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: decoding %s!\n", my_indirect_id, __func__, my_instruction->print());

        // Decoding the instruction
        my_base_addr = my_instruction->baseAddr;
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
        my_RT_config = getRowTableConfig(my_base_addr);

        // Initialization
        my_virtual_addr = 0;
        assert(my_outstanding_cache_read_pkts.size() == 0);
        // assert(my_outstanding_cache_evict_pkts.size() == 0);
        assert(my_outstanding_cpu_snoop_pkts.size() == 0);
        assert(my_outstanding_mem_read_pkts.size() == 0);
        assert(my_outstanding_mem_write_pkts.size() == 0);
        my_received_responses = my_expected_responses = 0;
        offset_table->reset();
        for (int i = 0; i < num_RT_banks[my_RT_config]; i++) {
            RT[my_RT_config][i].reset();
            my_RT_req_sent[my_RT_config][i] = false;
        }
        my_i = 0;
        my_max = -1;
        my_SPD_read_finish_tick = curTick();
        my_SPD_write_finish_tick = curTick();
        my_RT_access_finish_tick = curTick();
        my_decode_start_tick = curTick();
        my_fill_start_tick = 0;
        my_build_start_tick = 0;
        my_request_start_tick = 0;
        my_drain = false;

        // Setting the state of the instruction and stream unit
        my_instruction->state = Instruction::Status::Service;
        DPRINTF(MAAIndirect, "I[%d] %s: state set to Fill for request %s!\n", my_indirect_id, __func__, my_instruction->print());
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
            bool cond_ready = my_cond_tile == -1 || maa->spd->getElementFinished(my_cond_tile, my_i, 4, (uint8_t)FuncUnitType::INDIRECT, my_indirect_id);
            bool idx_ready = cond_ready && maa->spd->getElementFinished(my_idx_tile, my_i, 4, (uint8_t)FuncUnitType::INDIRECT, my_indirect_id);
            bool src_ready = idx_ready && (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD ||
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
                // Row table parallelism = total #sub-banks. Each bank can be inserted once at a cycle
                updateLatency(0, num_spd_read_condidx_accesses, 0, num_rowtable_accesses, total_num_RT_subbanks);
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
                my_RT_idx = getRowTableIdx(my_RT_config, addr_vec[ADDR_CHANNEL_LEVEL], addr_vec[ADDR_RANK_LEVEL],
                                           addr_vec[ADDR_BANKGROUP_LEVEL], addr_vec[ADDR_BANK_LEVEL]);
                Addr grow_addr = getGrowAddr(my_RT_config, addr_vec[ADDR_BANKGROUP_LEVEL], addr_vec[ADDR_BANK_LEVEL],
                                             addr_vec[ADDR_ROW_LEVEL]);
                DPRINTF(MAAIndirect, "I[%d] %s: inserting vaddr(0x%lx), paddr(0x%lx), MAP(RO: %d, BA: %d, BG: %d, RA: %d, CO: %d, CH: %d), grow(0x%lx), itr(%d), idx(%d), wid(%d) to T[%d]\n",
                        my_indirect_id, __func__, block_vaddr, block_paddr,
                        addr_vec[ADDR_ROW_LEVEL], addr_vec[ADDR_BANK_LEVEL], addr_vec[ADDR_BANKGROUP_LEVEL], addr_vec[ADDR_RANK_LEVEL], addr_vec[ADDR_COLUMN_LEVEL], addr_vec[ADDR_CHANNEL_LEVEL],
                        grow_addr, my_i, idx, wid, my_RT_idx);
                bool inserted = RT[my_RT_config][my_RT_idx].insert(grow_addr, block_paddr, my_i, wid);
                num_rowtable_accesses++;
                if (inserted == false) {
                    // Row table parallelism = total #sub-banks. Each bank can be inserted once at a cycle
                    updateLatency(0, num_spd_read_condidx_accesses, 0, num_rowtable_accesses, total_num_RT_subbanks);
                    scheduleNextExecution(true);
                    state = Status::Build;
                    (*maa->stats.IND_NumRTFull[my_indirect_id])++;
                    return;
                } else {
                    my_unique_WORD_addrs.insert(vaddr);
                    my_unique_CL_addrs.insert(block_paddr);
                    my_unique_ROW_addrs.insert(grow_addr + my_RT_idx * num_RT_possible_grows[my_RT_config]);
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

        // Row table parallelism = total #sub-banks. Each bank can be inserted once at a cycle
        updateLatency(0, num_spd_read_condidx_accesses, 0, num_rowtable_accesses, total_num_RT_subbanks);
        scheduleNextExecution(true);
        state = Status::Build;
        my_drain = true;
        return;
    }
    case Status::Build: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: Building %s requests, draining mode: %s!\n",
                my_indirect_id, __func__, my_instruction->print(), my_drain ? "true" : "false");
        if (scheduleNextExecution()) {
            break;
        }
        if (my_build_start_tick == 0) {
            my_build_start_tick = curTick();
        }
        if (my_fill_start_tick != 0) {
            (*maa->stats.IND_CyclesFill[my_indirect_id]) += maa->getTicksToCycles(curTick() - my_fill_start_tick);
            my_fill_start_tick = 0;
        }
        int last_RT_sent = 0;
        int num_rowtable_accesses = 0;
        Addr addr;
        while (true) {
            if (checkAndResetAllRowTablesSent())
                break;
            for (; last_RT_sent < num_RT_banks[my_RT_config]; last_RT_sent++) {
                int RT_idx = my_RT_bank_order[my_RT_config][last_RT_sent];
                assert(RT_idx < num_RT_banks[my_RT_config]);
                DPRINTF(MAAIndirect, "I[%d] %s: Checking row table bank[%d]!\n", my_indirect_id, __func__, RT_idx);
                if (my_RT_req_sent[my_RT_config][RT_idx] == false) {
                    if (RT[my_RT_config][RT_idx].get_entry_send(addr, my_drain)) {
                        DPRINTF(MAAIndirect, "I[%d] %s: Creating packet for bank[%d], addr[0x%lx]!\n", my_indirect_id, __func__, RT_idx, addr);
                        my_expected_responses++;
                        num_rowtable_accesses++;
                        createCacheSnoopPacket(addr, getCeiling(num_rowtable_accesses, total_num_RT_subbanks) * rowtable_latency);
                    } else {
                        DPRINTF(MAAIndirect, "I[%d] %s: T[%d] has nothing, setting sent to true!\n", my_indirect_id, __func__, RT_idx);
                        my_RT_req_sent[my_RT_config][RT_idx] = true;
                    }
                } else {
                    DPRINTF(MAAIndirect, "I[%d] %s: T[%d] has already sent the requests!\n", my_indirect_id, __func__, RT_idx);
                }
            }
            last_RT_sent = (last_RT_sent >= num_RT_banks[my_RT_config]) ? 0 : last_RT_sent;
        }
        DPRINTF(MAAIndirect, "I[%d] %s: state set to Request for %s!\n", my_indirect_id, __func__, my_instruction->print());
        // Row table parallelism = total #banks. Each bank can give us a address in a cycle.
        updateLatency(0, 0, 0, num_rowtable_accesses, total_num_RT_subbanks);
        state = Status::Request;
        scheduleNextExecution(true);
        scheduleNextSendCpu();
        break;
    }
    case Status::Request: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: requesting %s!\n", my_indirect_id, __func__, my_instruction->print());
        if (my_request_start_tick == 0) {
            my_request_start_tick = curTick();
        }
        if (my_build_start_tick != 0) {
            (*maa->stats.IND_CyclesBuild[my_indirect_id]) += maa->getTicksToCycles(curTick() - my_build_start_tick);
            my_build_start_tick = 0;
        }
        if (scheduleNextExecution()) {
            DPRINTF(MAAIndirect, "I[%d] %s: requesting is still not ready, returning!\n", my_indirect_id, __func__);
            break;
        }
        if (allPacketsSent() && my_received_responses == my_expected_responses) {
            if (my_drain) {
                state = Status::Response;
                my_drain = false;
            } else {
                state = Status::Fill;
            }
            DPRINTF(MAAIndirect, "I[%d] %s: all responses received, calling execution again in state %s!\n",
                    my_indirect_id, __func__, status_names[(int)state]);
            scheduleNextExecution(true);
        } else {
            DPRINTF(MAAIndirect, "I[%d] %s: waiting for %d responses, received %d responses!\n",
                    my_indirect_id, __func__, my_expected_responses, my_received_responses);
        }
        break;
    }
    case Status::Response: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAIndirect, "I[%d] %s: responding %s!\n", my_indirect_id, __func__, my_instruction->print());
        DPRINTF(MAATrace, "I[%d] End [%s]\n", my_indirect_id, my_instruction->print());
        panic_if(scheduleNextExecution(), "I[%d] %s: Execution is not completed!\n", my_indirect_id, __func__);
        panic_if(scheduleNextSendCache(), "I[%d] %s: Sending cache reads is not completed!\n", my_indirect_id, __func__);
        panic_if(scheduleNextSendCpu(), "I[%d] %s: Sending cpu snoop is not completed!\n", my_indirect_id, __func__);
        panic_if(scheduleNextSendMemRead(), "I[%d] %s: Sending mem reads is not completed!\n", my_indirect_id, __func__);
        panic_if(scheduleNextSendMemWrite(), "I[%d] %s: Sending mem writes is not completed!\n", my_indirect_id, __func__);
        panic_if(my_cond_tile_ready == false, "I[%d] %s: cond tile[%d] is not ready!\n", my_indirect_id, __func__, my_cond_tile);
        panic_if(my_idx_tile_ready == false, "I[%d] %s: idx tile[%d] is not ready!\n", my_indirect_id, __func__, my_idx_tile);
        panic_if(my_src_tile_ready == false, "I[%d] %s: src tile[%d] is not ready!\n", my_indirect_id, __func__, my_src_tile);
        panic_if(LoadsCacheHitRespondingTimeHistory.size() != 0, "I[%d] %s: LoadsCacheHitRespondingTimeHistory is not empty!\n", my_indirect_id, __func__);
        panic_if(LoadsCacheHitAccessingTimeHistory.size() != 0, "I[%d] %s: LoadsCacheHitAccessingTimeHistory is not empty!\n", my_indirect_id, __func__);
        panic_if(LoadsMemAccessingTimeHistory.size() != 0, "I[%d] %s: LoadsMemAccessingTimeHistory is not empty!\n", my_indirect_id, __func__);
        DPRINTF(MAAIndirect, "I[%d] %s: state set to finish for request %s!\n", my_indirect_id, __func__, my_instruction->print());
        my_instruction->state = Instruction::Status::Finish;
        if (my_request_start_tick != 0) {
            (*maa->stats.IND_CyclesRequest[my_indirect_id]) += maa->getTicksToCycles(curTick() - my_request_start_tick);
            my_request_start_tick = 0;
        }
        Cycles total_cycles = maa->getTicksToCycles(curTick() - my_decode_start_tick);
        maa->stats.cycles += total_cycles;
        my_decode_start_tick = 0;
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
        setRowTableConfig(my_base_addr, my_unique_CL_addrs.size(), my_unique_ROW_addrs.size());
        (*maa->stats.IND_NumUniqueWordsInserted[my_indirect_id]) += my_unique_WORD_addrs.size();
        (*maa->stats.IND_NumUniqueCacheLineInserted[my_indirect_id]) += my_unique_CL_addrs.size();
        (*maa->stats.IND_NumUniqueRowsInserted[my_indirect_id]) += my_unique_ROW_addrs.size();
        my_unique_WORD_addrs.clear();
        my_unique_CL_addrs.clear();
        my_unique_ROW_addrs.clear();
        my_instruction = nullptr;
        break;
    }
    default:
        assert(false);
    }
}
bool IndirectAccessUnit::checkAndResetAllRowTablesSent() {
    for (int i = 0; i < num_RT_banks[my_RT_config]; i++) {
        if (my_RT_req_sent[my_RT_config][i] == false) {
            return false;
        }
    }
    for (int i = 0; i < num_RT_banks[my_RT_config]; i++) {
        my_RT_req_sent[my_RT_config][i] = false;
    }
    return true;
}

void IndirectAccessUnit::createCacheReadPacket(Addr addr) {
    /**** Packet generation ****/
    RequestPtr real_req = std::make_shared<Request>(addr, block_size, flags, maa->requestorId);
    PacketPtr read_pkt;
    if (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD) {
        read_pkt = new Packet(real_req, MemCmd::ReadSharedReq);
    } else {
        read_pkt = new Packet(real_req, MemCmd::ReadExReq);
    }
    read_pkt->allocate();
    IndirectAccessUnit::IndirectPacket new_packet = IndirectAccessUnit::IndirectPacket(read_pkt, maa->getClockEdge(Cycles(0)));
    my_outstanding_cache_read_pkts.insert(new_packet);
    DPRINTF(MAAIndirect, "I[%d] %s: created %s to send for cache\n", my_indirect_id, __func__, read_pkt->print(), new_packet.tick);
}
void IndirectAccessUnit::createCacheSnoopPacket(Addr addr, int latency) {
    /**** Packet generation ****/
    RequestPtr real_req = std::make_shared<Request>(addr, block_size, flags, maa->requestorId);
    PacketPtr snoop_pkt;
    if (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD) {
        snoop_pkt = new Packet(real_req, MemCmd::ReadSharedReq);
    } else {
        snoop_pkt = new Packet(real_req, MemCmd::ReadExReq);
    }
    snoop_pkt->allocate();
    snoop_pkt->setExpressSnoop();
    snoop_pkt->headerDelay = snoop_pkt->payloadDelay = 0;
    IndirectAccessUnit::IndirectPacket new_packet = IndirectAccessUnit::IndirectPacket(snoop_pkt, maa->getClockEdge(Cycles(latency)));
    my_outstanding_cpu_snoop_pkts.insert(new_packet);
    DPRINTF(MAAIndirect, "I[%d] %s: created %s to send in %d cycles at tick %u, for cache\n", my_indirect_id, __func__, snoop_pkt->print(), latency, new_packet.tick);
}
// void IndirectAccessUnit::createCacheEvictPacket(Addr addr, int core_id) {
//     /**** Packet generation ****/
//     RequestPtr real_req = std::make_shared<Request>(addr, block_size, flags, maa->requestorId);
//     PacketPtr my_pkt = new Packet(real_req, MemCmd::CleanEvict);
//     IndirectAccessUnit::IndirectPacket new_packet = IndirectAccessUnit::IndirectPacket(my_pkt, maa->getClockEdge(Cycles(0)), core_id);
//     my_outstanding_cache_evict_pkts.insert(new_packet);
//     DPRINTF(MAAIndirect, "I[%d] %s: created %s to send in %d cycles\n", my_indirect_id, __func__, my_pkt->print(), 0);
// }
void IndirectAccessUnit::createMemReadPacket(Addr addr) {
    /**** Packet generation ****/
    RequestPtr real_req = std::make_shared<Request>(addr, block_size, flags, maa->requestorId);
    PacketPtr read_pkt;
    if (my_instruction->opcode == Instruction::OpcodeType::INDIR_LD) {
        read_pkt = new Packet(real_req, MemCmd::ReadSharedReq);
    } else {
        read_pkt = new Packet(real_req, MemCmd::ReadExReq);
    }
    read_pkt->allocate();
    IndirectAccessUnit::IndirectPacket new_packet = IndirectAccessUnit::IndirectPacket(read_pkt, maa->getClockEdge(Cycles(0)));
    my_outstanding_mem_read_pkts.insert(new_packet);
    DPRINTF(MAAIndirect, "I[%d] %s: created %s for mem\n", my_indirect_id, __func__, read_pkt->print());
}
bool IndirectAccessUnit::sendOutstandingCpuPacket() {
    bool mem_packet_pushed = false;
    bool cache_packet_pushed = false;
    bool snoop_packet_remained = false;

    DPRINTF(MAAIndirect, "I[%d] %s: sending %d outstanding cpu snoop packets...\n", my_indirect_id, __func__, my_outstanding_cpu_snoop_pkts.size());
    while (my_outstanding_cpu_snoop_pkts.empty() == false) {
        IndirectAccessUnit::IndirectPacket snoop_pkt = *my_outstanding_cpu_snoop_pkts.begin();
        DPRINTF(MAAIndirect, "I[%d] %s: trying sending snoop %s to cpuSide at time %u\n", my_indirect_id, __func__, snoop_pkt.packet->print(), snoop_pkt.tick);
        if (snoop_pkt.tick > curTick()) {
            DPRINTF(MAAIndirect, "I[%d] %s: waiting for %d cycles\n", my_indirect_id, __func__, maa->getTicksToCycles(snoop_pkt.tick - curTick()));
            snoop_packet_remained = true;
            break;
        }
        DPRINTF(MAAIndirect, "I[%d] %s: trying sending snoop %s to cpuSide\n", my_indirect_id, __func__, snoop_pkt.packet->print());
        if (maa->sendSnoopPacketCpu((uint8_t)FuncUnitType::INDIRECT, my_indirect_id, snoop_pkt.packet) == false) {
            DPRINTF(MAAIndirect, "I[%d] %s: send failed, leaving send packet...\n", my_indirect_id, __func__);
            break;
        }
        DPRINTF(MAAIndirect, "I[%d] %s: successfully sent as a snoop to cpuSide, cache responding: %s, has sharers %s, had writable %s, satisfied %s, is block cached %s...\n",
                my_indirect_id, __func__, snoop_pkt.packet->cacheResponding(), snoop_pkt.packet->hasSharers(), snoop_pkt.packet->responderHadWritable(), snoop_pkt.packet->satisfied(), snoop_pkt.packet->isBlockCached());
        my_outstanding_cpu_snoop_pkts.erase(my_outstanding_cpu_snoop_pkts.begin());
        if (snoop_pkt.packet->cacheResponding() == true) {
            DPRINTF(MAAIndirect, "I[%d] %s: a cache in the O/M state will respond, send successfull...\n", my_indirect_id, __func__);
            (*maa->stats.IND_LoadsCacheHitResponding[my_indirect_id])++;
            LoadsCacheHitRespondingTimeHistory[snoop_pkt.packet->getAddr()] = curTick();
        } else if (snoop_pkt.packet->hasSharers() == true) {
            DPRINTF(MAAIndirect, "I[%d] %s: There's a cache in the E/S state will respond, creating again and sending to cache\n", my_indirect_id, __func__);
            panic_if(snoop_pkt.packet->needsWritable(), "I[%d] %s: packet needs writable!\n", my_indirect_id, __func__);
            createCacheReadPacket(snoop_pkt.packet->getAddr());
            cache_packet_pushed = true;
            (*maa->stats.IND_LoadsCacheHitAccessing[my_indirect_id])++;
        } else {
            DPRINTF(MAAIndirect, "I[%d] %s: no cache responds (I), creating again and sending to memory\n", my_indirect_id, __func__);
            createMemReadPacket(snoop_pkt.packet->getAddr());
            mem_packet_pushed = true;
            (*maa->stats.IND_LoadsMemAccessing[my_indirect_id])++;
        }
    }

    if (snoop_packet_remained) {
        scheduleNextSendCpu();
    }
    if (cache_packet_pushed) {
        scheduleNextSendCache();
    }
    if (mem_packet_pushed) {
        scheduleNextSendMemRead();
    }
    return true;
}
bool IndirectAccessUnit::sendOutstandingCachePacket() {
    // bool read_packet_blocked = false;
    // bool evict_packet_sent = false;

    DPRINTF(MAAIndirect, "I[%d] %s: sending %d outstanding cache read packets...\n", my_indirect_id, __func__, my_outstanding_cache_read_pkts.size());
    while (my_outstanding_cache_read_pkts.empty() == false) {
        IndirectAccessUnit::IndirectPacket read_pkt = *my_outstanding_cache_read_pkts.begin();
        DPRINTF(MAAIndirect, "I[%d] %s: trying sending %s to cacheSide\n", my_indirect_id, __func__, read_pkt.packet->print());
        panic_if(read_pkt.tick > curTick(), "I[%d] %s: waiting for %d cycles\n", my_indirect_id, __func__, maa->getTicksToCycles(read_pkt.tick - curTick()));
        panic_if(read_pkt.packet->needsResponse() == false, "I[%d] %s: packet does not need response!\n", my_indirect_id, __func__);
        if (maa->sendPacketCache((uint8_t)FuncUnitType::INDIRECT, my_indirect_id, read_pkt.packet) == false) {
            DPRINTF(MAAIndirect, "I[%d] %s: send failed, leaving send packet...\n", my_indirect_id, __func__);
            // read_packet_blocked = true;
            break;
        } else {
            LoadsCacheHitAccessingTimeHistory[read_pkt.packet->getAddr()] = curTick();
            my_outstanding_cache_read_pkts.erase(my_outstanding_cache_read_pkts.begin());
        }
    }

    // DPRINTF(MAAIndirect, "I[%d] %s: sending %d outstanding cache evict packets...\n", my_indirect_id, __func__, my_outstanding_cache_evict_pkts.size());
    // while (my_outstanding_cache_evict_pkts.empty() == false && read_packet_blocked == false) {
    //     IndirectAccessUnit::IndirectPacket evict_pkt = *my_outstanding_cache_evict_pkts.begin();
    //     DPRINTF(MAAIndirect, "I[%d] %s: trying sending %s to cacheSide\n", my_indirect_id, __func__, evict_pkt.packet->print());
    //     panic_if(evict_pkt.tick > curTick(), "I[%d] %s: waiting for %d cycles\n", my_indirect_id, __func__, maa->getTicksToCycles(evict_pkt.tick - curTick()));
    //     panic_if(evict_pkt.packet->needsResponse(), "I[%d] %s: packet needs response!\n", my_indirect_id, __func__);
    //     if (maa->sendPacketCache((uint8_t)FuncUnitType::INDIRECT, my_indirect_id, evict_pkt.packet, evict_pkt.core_id) == false) {
    //         DPRINTF(MAAIndirect, "I[%d] %s: send failed, leaving send packet...\n", my_indirect_id, __func__);
    //         break;
    //     } else {
    //         evict_packet_sent = true;
    //         my_outstanding_cache_evict_pkts.erase(my_outstanding_cache_evict_pkts.begin());
    //         // A packet is sent successfully, we can continue sending the next packets
    //     }
    // }

    if (allPacketsSent() && (my_received_responses == my_expected_responses)) { // evict_packet_sent &&
        DPRINTF(MAAIndirect, "I[%d] %s: all responses received, calling execution again in state %s!\n", my_indirect_id, __func__, status_names[(int)state]);
        scheduleNextExecution(true);
    } else {
        DPRINTF(MAAIndirect, "I[%d] %s: expected: %d, received: %d!\n", my_indirect_id, __func__, my_expected_responses, my_received_responses);
    }
    return true;
}
bool IndirectAccessUnit::sendOutstandingMemReadPacket() {
    DPRINTF(MAAIndirect, "I[%d] %s: sending %d outstanding mem read packets...\n", my_indirect_id, __func__, my_outstanding_mem_read_pkts.size());
    for (auto it = my_outstanding_mem_read_pkts.begin(); it != my_outstanding_mem_read_pkts.end();) {
        IndirectAccessUnit::IndirectPacket read_pkt = *it;
        int channel_addr = maa->channel_addr(read_pkt.packet->getAddr());
        if (mem_channels_blocked[channel_addr]) {
            ++it;
            continue;
        }
        DPRINTF(MAAIndirect, "I[%d] %s: trying sending %s to memSide\n", my_indirect_id, __func__, read_pkt.packet->print());
        panic_if(read_pkt.tick > curTick(), "I[%d] %s: waiting for %d cycles\n", my_indirect_id, __func__, maa->getTicksToCycles(read_pkt.tick - curTick()));
        if (maa->sendPacketMem(my_indirect_id, read_pkt.packet) == false) {
            DPRINTF(MAAIndirect, "I[%d] %s: send failed for channel %d...\n", my_indirect_id, __func__, channel_addr);
            mem_channels_blocked[channel_addr] = true;
            ++it;
            continue;
        } else {
            if (read_pkt.packet->needsResponse()) {
                LoadsMemAccessingTimeHistory[read_pkt.packet->getAddr()] = curTick();
            }
            it = my_outstanding_mem_read_pkts.erase(it);
            continue;
        }
    }
    return true;
}
bool IndirectAccessUnit::sendOutstandingMemWritePacket() {
    bool write_packet_sent = false;
    for (auto it = my_outstanding_mem_write_pkts.begin(); it != my_outstanding_mem_write_pkts.end();) {
        IndirectAccessUnit::IndirectPacket write_pkt = *it;
        if (write_pkt.tick > curTick()) {
            DPRINTF(MAAIndirect, "I[%d] %s: waiting for %d cycles to send %s to memory\n", my_indirect_id, __func__, maa->getTicksToCycles(write_pkt.tick - curTick()), write_pkt.packet->print());
            scheduleNextSendMemWrite();
            return false;
        }
        int channel_addr = maa->channel_addr(write_pkt.packet->getAddr());
        if (mem_channels_blocked[channel_addr]) {
            ++it;
            continue;
        }
        DPRINTF(MAAIndirect, "I[%d] %s: trying sending %s to memory\n", my_indirect_id, __func__, write_pkt.packet->print());
        if (maa->sendPacketMem(my_indirect_id, write_pkt.packet) == false) {
            DPRINTF(MAAIndirect, "I[%d] %s: send failed for channel %d\n", my_indirect_id, __func__, channel_addr);
            mem_channels_blocked[channel_addr] = true;
            ++it;
            continue;
        } else {
            it = my_outstanding_mem_write_pkts.erase(it);
            my_received_responses++;
            write_packet_sent = true;
            continue;
        }
    }
    if (write_packet_sent && allPacketsSent() && (my_received_responses == my_expected_responses)) {
        DPRINTF(MAAIndirect, "I[%d] %s: all responses received, calling execution again in state %s!\n", my_indirect_id, __func__, status_names[(int)state]);
        scheduleNextExecution(true);
    } else {
        DPRINTF(MAAIndirect, "I[%d] %s: expected: %d, received: %d, packet send: %d!\n", my_indirect_id, __func__, my_expected_responses, my_received_responses, write_packet_sent);
    }
    return true;
}
bool IndirectAccessUnit::allPacketsSent() {
    return my_outstanding_cache_read_pkts.empty() &&
           //    my_outstanding_cache_evict_pkts.empty() &&
           my_outstanding_cpu_snoop_pkts.empty() &&
           my_outstanding_mem_read_pkts.empty() &&
           my_outstanding_mem_write_pkts.empty();
}
void IndirectAccessUnit::unblockMemChannel(int channel_addr) {
    panic_if(mem_channels_blocked[channel_addr] == false, "I[%d] %s: channel %d is not blocked!\n", my_indirect_id, __func__, channel_addr);
    mem_channels_blocked[channel_addr] = false;
}
bool IndirectAccessUnit::recvData(const Addr addr, uint8_t *dataptr, bool is_block_cached, int core_id) {
    std::vector addr_vec = maa->map_addr(addr);
    int RT_idx = getRowTableIdx(my_RT_config, addr_vec[ADDR_CHANNEL_LEVEL], addr_vec[ADDR_RANK_LEVEL],
                                addr_vec[ADDR_BANKGROUP_LEVEL], addr_vec[ADDR_BANK_LEVEL]);
    Addr grow_addr = getGrowAddr(my_RT_config, addr_vec[ADDR_BANKGROUP_LEVEL], addr_vec[ADDR_BANK_LEVEL],
                                 addr_vec[ADDR_ROW_LEVEL]);
    std::vector<OffsetTableEntry> entries = RT[my_RT_config][RT_idx].get_entry_recv(grow_addr, addr);
    DPRINTF(MAAIndirect, "I[%d] %s: %d entries received for addr(0x%lx), grow(x%lx) from T[%d]!\n",
            my_indirect_id, __func__, entries.size(), addr, grow_addr, RT_idx);
    if (entries.size() == 0) {
        return false;
    }
    // bool cache_response = false;
    if (is_block_cached) {
        if (LoadsCacheHitRespondingTimeHistory.find(addr) != LoadsCacheHitRespondingTimeHistory.end()) {
            (*maa->stats.IND_LoadsCacheHitRespondingLatency[my_indirect_id]) += maa->getTicksToCycles(curTick() - LoadsCacheHitRespondingTimeHistory[addr]);
            LoadsCacheHitRespondingTimeHistory.erase(addr);
        } else if (LoadsCacheHitAccessingTimeHistory.find(addr) != LoadsCacheHitAccessingTimeHistory.end()) {
            (*maa->stats.IND_LoadsCacheHitAccessingLatency[my_indirect_id]) += maa->getTicksToCycles(curTick() - LoadsCacheHitAccessingTimeHistory[addr]);
            LoadsCacheHitAccessingTimeHistory.erase(addr);
            // cache_response = true;
        } else {
            panic("I[%d] %s: addr(0x%lx) is not in the cache hit history!\n", my_indirect_id, __func__, addr);
        }
    } else {
        panic_if(LoadsMemAccessingTimeHistory.find(addr) == LoadsMemAccessingTimeHistory.end(), "I[%d] %s: addr(0x%lx) is not in the memory accessing history!\n", my_indirect_id, __func__, addr);
        (*maa->stats.IND_LoadsMemAccessingLatency[my_indirect_id]) += maa->getTicksToCycles(curTick() - LoadsMemAccessingTimeHistory[addr]);
        LoadsMemAccessingTimeHistory.erase(addr);
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

    // Row table parallelism = total #banks.
    // We will have total #banks offset table walkers.
    Cycles total_latency = updateLatency(num_recv_spd_read_accesses, 0, num_recv_spd_write_accesses, num_recv_rt_accesses, total_num_RT_subbanks);
    if (my_instruction->opcode == Instruction::OpcodeType::INDIR_ST || my_instruction->opcode == Instruction::OpcodeType::INDIR_RMW) {
        RequestPtr real_req = std::make_shared<Request>(addr, block_size, flags, maa->requestorId);
        PacketPtr write_pkt = new Packet(real_req, MemCmd::WritebackDirty);
        write_pkt->allocate();
        write_pkt->setData(new_data);
        for (int i = 0; i < block_size / my_word_size; i++) {
            if (my_word_size == 4)
                DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] = %f!\n", my_indirect_id, __func__, i, write_pkt->getPtr<float>()[i]);
            else
                DPRINTF(MAAIndirect, "I[%d] %s: new_data[%d] = %f!\n", my_indirect_id, __func__, i, write_pkt->getPtr<double>()[i]);
        }
        DPRINTF(MAAIndirect, "I[%d] %s: created %s to send in %d cycles\n", my_indirect_id, __func__, write_pkt->print(), total_latency);
        my_outstanding_mem_write_pkts.insert(IndirectAccessUnit::IndirectPacket(write_pkt, maa->getClockEdge(total_latency)));
        (*maa->stats.IND_StoresMemAccessing[my_indirect_id])++;
        scheduleNextSendMemWrite();
    } else {
        my_received_responses++;
        // if (cache_response) {
        //     createCacheEvictPacket(addr, core_id);
        //     (*maa->stats.IND_Evicts[my_indirect_id])++;
        //     scheduleNextSendCache();
        // } else
        if (allPacketsSent() && my_received_responses == my_expected_responses) {
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
void IndirectAccessUnit::finish(const Fault &fault, const RequestPtr &req, ThreadContext *tc, BaseMMU::Mode mode) {
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
void IndirectAccessUnit::scheduleSendCpuPacketEvent(int latency) {
    DPRINTF(MAAIndirect, "I[%d] %s: scheduling send cpu read packet for the Indirect Unit in the next %d cycles!\n", my_indirect_id, __func__, latency);
    panic_if(latency < 0, "Negative latency of %d!\n", latency);
    Tick new_when = maa->getClockEdge(Cycles(latency));
    if (!sendCpuPacketEvent.scheduled()) {
        maa->schedule(sendCpuPacketEvent, new_when);
    } else {
        Tick old_when = sendCpuPacketEvent.when();
        DPRINTF(MAAIndirect, "I[%d] %s: send cpu packet already scheduled for tick %d\n", my_indirect_id, __func__, old_when);
        if (new_when < old_when) {
            DPRINTF(MAAIndirect, "I[%d] %s: rescheduling for tick %d!\n", my_indirect_id, __func__, new_when);
            maa->reschedule(sendCpuPacketEvent, new_when);
        }
    }
}
void IndirectAccessUnit::scheduleSendCachePacketEvent(int latency) {
    DPRINTF(MAAIndirect, "I[%d] %s: scheduling send cache read packet for the Indirect Unit in the next %d cycles!\n", my_indirect_id, __func__, latency);
    panic_if(latency < 0, "Negative latency of %d!\n", latency);
    Tick new_when = maa->getClockEdge(Cycles(latency));
    if (!sendCachePacketEvent.scheduled()) {
        maa->schedule(sendCachePacketEvent, new_when);
    } else {
        Tick old_when = sendCachePacketEvent.when();
        DPRINTF(MAAIndirect, "I[%d] %s: send cache packet already scheduled for tick %d\n", my_indirect_id, __func__, old_when);
        if (new_when < old_when) {
            DPRINTF(MAAIndirect, "I[%d] %s: rescheduling for tick %d!\n", my_indirect_id, __func__, new_when);
            maa->reschedule(sendCachePacketEvent, new_when);
        }
    }
}
void IndirectAccessUnit::scheduleSendMemReadPacketEvent(int latency) {
    DPRINTF(MAAIndirect, "I[%d] %s: scheduling send mem read packet for the Indirect Unit in the next %d cycles!\n", my_indirect_id, __func__, latency);
    panic_if(latency < 0, "Negative latency of %d!\n", latency);
    Tick new_when = maa->getClockEdge(Cycles(latency));
    if (!sendMemReadPacketEvent.scheduled()) {
        maa->schedule(sendMemReadPacketEvent, new_when);
    } else {
        Tick old_when = sendMemReadPacketEvent.when();
        DPRINTF(MAAIndirect, "I[%d] %s: send packet already scheduled for tick %d\n", my_indirect_id, __func__, old_when);
        if (new_when < old_when) {
            DPRINTF(MAAIndirect, "I[%d] %s: rescheduling for tick %d!\n", my_indirect_id, __func__, new_when);
            maa->reschedule(sendMemReadPacketEvent, new_when);
        }
    }
}
void IndirectAccessUnit::scheduleSendMemWritePacketEvent(int latency) {
    DPRINTF(MAAIndirect, "I[%d] %s: scheduling send mem write packet for the Indirect Unit in the next %d cycles!\n", my_indirect_id, __func__, latency);
    panic_if(latency < 0, "Negative latency of %d!\n", latency);
    Tick new_when = maa->getClockEdge(Cycles(latency));
    if (!sendMemWritePacketEvent.scheduled()) {
        maa->schedule(sendMemWritePacketEvent, new_when);
    } else {
        Tick old_when = sendMemWritePacketEvent.when();
        DPRINTF(MAAIndirect, "I[%d] %s: send packet already scheduled for tick %d\n", my_indirect_id, __func__, old_when);
        if (new_when < old_when) {
            DPRINTF(MAAIndirect, "I[%d] %s: rescheduling for tick %d!\n", my_indirect_id, __func__, new_when);
            maa->reschedule(sendMemWritePacketEvent, new_when);
        }
    }
}
} // namespace gem5