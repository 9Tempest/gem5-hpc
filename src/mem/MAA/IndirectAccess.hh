#ifndef __MEM_MAA_INDIRECT_ACCESS_HH__
#define __MEM_MAA_INDIRECT_ACCESS_HH__

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>

#include "base/statistics.hh"
#include "base/types.hh"
#include "mem/packet.hh"
#include "mem/request.hh"
#include "sim/system.hh"
#include "arch/generic/mmu.hh"

#define ADDR_CHANNEL_LEVEL   0
#define ADDR_RANK_LEVEL      1
#define ADDR_BANKGROUP_LEVEL 2
#define ADDR_BANK_LEVEL      3
#define ADDR_ROW_LEVEL       4
#define ADDR_COLUMN_LEVEL    5

namespace gem5 {

class MAA;
class IndirectAccessUnit;
class Instruction;

struct OffsetTableEntry {
    int itr;
    int wid;
    int next_itr;
};
class OffsetTable {
public:
    OffsetTable() {
        entries = nullptr;
        entries_valid = nullptr;
    }
    ~OffsetTable() {
        if (entries != nullptr) {
            delete[] entries;
            assert(entries_valid != nullptr);
            delete[] entries_valid;
        }
    }
    void allocate(int _my_indirect_id,
                  int _num_tile_elements,
                  IndirectAccessUnit *_indir_access);
    void insert(int itr, int wid, int last_itr);
    std::vector<OffsetTableEntry> get_entry_recv(int first_itr);
    void reset();
    void check_reset();
    OffsetTableEntry *entries;
    bool *entries_valid;
    int num_tile_elements;
    IndirectAccessUnit *indir_access;
    int my_indirect_id;
};

class RowTableEntry {
public:
    struct Entry {
        Addr addr;
        int first_itr;
        int last_itr;
    };
    RowTableEntry() {
        entries = nullptr;
        entries_valid = nullptr;
    }
    ~RowTableEntry() {
        if (entries != nullptr) {
            delete[] entries;
            assert(entries_valid != nullptr);
            delete[] entries_valid;
        }
    }
    void allocate(int _my_indirect_id,
                  int _my_table_id,
                  int _my_table_row_id,
                  int _num_row_table_entries_per_row,
                  OffsetTable *_offset_table,
                  IndirectAccessUnit *_indir_access);
    bool insert(Addr addr, int itr, int wid);
    bool find_addr(Addr addr);
    void reset();
    void check_reset();
    bool get_entry_send(Addr &addr);
    std::vector<OffsetTableEntry> get_entry_recv(Addr addr);
    bool all_entries_received();
    OffsetTable *offset_table;
    Addr Grow_addr;
    Entry *entries;
    bool *entries_valid;
    int num_row_table_entries_per_row;
    int last_sent_entry_id;
    IndirectAccessUnit *indir_access;
    int my_indirect_id, my_table_id, my_table_row_id;
};
class RowTable {
public:
    RowTable() {
        entries = nullptr;
        entries_valid = nullptr;
    }
    ~RowTable() {
        if (entries != nullptr) {
            delete[] entries;
            assert(entries_valid != nullptr);
            delete[] entries_valid;
        }
    }
    void allocate(int _my_indirect_id,
                  int _my_table_id,
                  int _num_row_table_rows,
                  int _num_row_table_entries_per_row,
                  OffsetTable *_offset_table,
                  IndirectAccessUnit *_indir_access);
    bool insert(Addr Grow_addr,
                Addr addr,
                int itr,
                int wid);
    bool get_entry_send(Addr &addr);
    bool get_entry_send_first_row(Addr &addr);
    std::vector<OffsetTableEntry> get_entry_recv(Addr Grow_addr,
                                                 Addr addr);

    void reset();
    void check_reset();
    float getAverageEntriesPerRow();
    OffsetTable *offset_table;
    RowTableEntry *entries;
    bool *entries_valid;
    int num_row_table_rows;
    int num_row_table_entries_per_row;
    int last_sent_row_id;
    IndirectAccessUnit *indir_access;
    int my_indirect_id, my_table_id;
};

class IndirectAccessUnit : public BaseMMU::Translation {
public:
    enum class Status : uint8_t {
        Idle = 0,
        Decode = 1,
        Fill = 2,
        Drain = 3,
        Build = 4,
        Request = 5,
        Response = 6,
        max
    };

protected:
    std::string status_names[8] = {
        "Idle",
        "Decode",
        "Fill",
        "Drain",
        "Build",
        "Request",
        "Response",
        "max"};
    class IndirectPacket {
    public:
        PacketPtr packet;
        bool is_cached;
        bool is_snoop;
        Tick tick;
        IndirectPacket(PacketPtr _packet, bool _is_cached, bool _is_snoop, Tick _tick)
            : packet(_packet), is_cached(_is_cached), is_snoop(_is_snoop), tick(_tick) {}
        IndirectPacket(const IndirectPacket &other) {
            packet = other.packet;
            is_cached = other.is_cached;
            is_snoop = other.is_snoop;
            tick = other.tick;
        }
        bool operator<(const IndirectPacket &rhs) const {
            return tick < rhs.tick;
        }
    };
    struct CompareByTick {
        bool operator()(const IndirectPacket &lhs, const IndirectPacket &rhs) const {
            return lhs.tick < rhs.tick;
        }
    };
    int num_row_table_banks;
    int num_tile_elements;
    int num_row_table_rows;
    int num_row_table_entries_per_row;
    Status state, prev_state;
    RowTable *row_table;
    OffsetTable *offset_table;
    int dst_tile_id;
    Cycles rowtable_latency;
    Cycles cache_snoop_latency;

public:
    MAA *maa;
    IndirectAccessUnit();
    ~IndirectAccessUnit() {
        if (row_table != nullptr) {
            delete row_table;
        }
    }
    void allocate(int _my_indirect_id,
                  int _num_tile_elements,
                  int _num_row_table_rows,
                  int _num_row_table_entries_per_row,
                  Cycles _rowtable_latency,
                  Cycles _cache_snoop_latency,
                  MAA *_maa);
    Status getState() const { return state; }
    void scheduleExecuteInstructionEvent(int latency = 0);
    void scheduleSendReadPacketEvent(int latency = 0);
    void scheduleSendWritePacketEvent(int latency = 0);
    void setInstruction(Instruction *_instruction);

    bool recvData(const Addr addr,
                  uint8_t *dataptr,
                  bool is_block_cached);

    /* Related to BaseMMU::Translation Inheretance */
    void markDelayed() override {}
    void finish(const Fault &fault, const RequestPtr &req,
                ThreadContext *tc, BaseMMU::Mode mode) override;

protected:
    Instruction *my_instruction;
    std::multiset<IndirectPacket, CompareByTick> my_outstanding_write_pkts;
    std::multiset<IndirectPacket, CompareByTick> my_outstanding_read_pkts;
    Request::Flags flags = 0;
    const Addr block_size = 64;
    int my_word_size = -1;
    int my_words_per_cl = -1;
    Addr my_virtual_addr = 0;
    Addr my_base_addr;
    int my_dst_tile, my_src_tile, my_cond_tile, my_max, my_idx_tile;
    int my_expected_responses;
    int my_received_responses;
    std::vector<int> my_sorted_indices;
    bool *my_row_table_req_sent;
    std::vector<int> my_row_table_bank_order;
    int my_i, my_row_table_idx;

    bool my_translation_done;
    Addr my_translated_addr;
    int my_indirect_id;
    Tick my_SPD_read_finish_tick;
    Tick my_SPD_write_finish_tick;
    Tick my_RT_access_finish_tick;
    Tick my_decode_start_tick;
    Tick my_fill_start_tick;
    Tick my_drain_start_tick;
    Tick my_build_start_tick;
    Tick my_request_start_tick;

    Addr translatePacket(Addr vaddr);
    bool checkAllRowTablesSent();
    int getRowTableIdx(int channel, int rank, int bankgroup);
    void executeInstruction();
    EventFunctionWrapper executeInstructionEvent;
    EventFunctionWrapper sendReadPacketEvent;
    EventFunctionWrapper sendWritePacketEvent;
    void check_reset();
    bool scheduleNextExecution(bool force = false);
    bool scheduleNextSendRead();
    bool scheduleNextSendWrite();

public:
    void createReadPacket(Addr addr, int latency, bool is_cached, bool is_snoop);
    void createReadPacketEvict(Addr addr);
    bool sendOutstandingReadPacket();
    bool sendOutstandingWritePacket();
};
} // namespace gem5

#endif //__MEM_MAA_INDIRECT_ACCESS_HH__
