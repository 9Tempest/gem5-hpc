#ifndef __MEM_MAA_INDIRECT_ACCESS_HH__
#define __MEM_MAA_INDIRECT_ACCESS_HH__

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#include <set>

#include "base/statistics.hh"
#include "base/types.hh"
#include "mem/packet.hh"
#include "mem/request.hh"
#include "sim/system.hh"
#include "arch/generic/mmu.hh"

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
                  int _num_RT_entries_per_row,
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
    Addr grow_addr;
    Entry *entries;
    bool *entries_valid;
    int num_RT_entries_per_row;
    int last_sent_entry_id;
    IndirectAccessUnit *indir_access;
    int my_indirect_id, my_table_id, my_table_row_id;
};
class RowTable {
public:
    RowTable() {
        entries = nullptr;
        entries_valid = nullptr;
        entries_full = nullptr;
    }
    ~RowTable() {
        if (entries != nullptr) {
            delete[] entries;
            assert(entries_valid != nullptr);
            delete[] entries_valid;
            assert(entries_full != nullptr);
            delete[] entries_full;
        }
    }
    void allocate(int _my_indirect_id,
                  int _my_table_id,
                  int _num_RT_rows_per_bank,
                  int _num_RT_entries_per_row,
                  OffsetTable *_offset_table,
                  IndirectAccessUnit *_indir_access);
    bool insert(Addr grow_addr,
                Addr addr,
                int itr,
                int wid);
    bool get_entry_send(Addr &addr, bool drain);
    std::vector<OffsetTableEntry> get_entry_recv(Addr grow_addr,
                                                 Addr addr);

    void reset();
    void check_reset();
    float getAverageEntriesPerRow();
    OffsetTable *offset_table;
    RowTableEntry *entries;
    bool *entries_valid;
    bool *entries_full;
    int num_RT_rows_per_bank;
    int num_RT_entries_per_row;
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
        Build = 3,
        Request = 4,
        Response = 5,
        max
    };

protected:
    std::string status_names[7] = {
        "Idle",
        "Decode",
        "Fill",
        "Build",
        "Request",
        "Response",
        "max"};
    class IndirectPacket {
    public:
        PacketPtr packet;
        Tick tick;
        IndirectPacket(PacketPtr _packet, Tick _tick)
            : packet(_packet), tick(_tick) {}
        IndirectPacket(const IndirectPacket &other) {
            packet = other.packet;
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
    int total_num_RT_subbanks;
    int num_RT_configs;
    int my_RT_config;
    int initial_RT_config;
    int **RT_bank_org;
    int *num_RT_banks;
    int *num_RT_rows_total;
    Addr *num_RT_possible_grows;
    int *num_RT_subbanks;
    int *num_RT_bank_columns;
    Addr *RT_config_addr;
    int *RT_config_cache;
    Tick *RT_config_cache_tick;
    int num_tile_elements;
    int num_RT_rows_per_bank;
    int num_RT_entries_per_subbank_row;
    int num_RT_config_cache_entries;
    int num_channels;
    bool *mem_channels_blocked;
    Status state;
    RowTable **RT;
    OffsetTable *offset_table;
    int dst_tile_id;
    Cycles rowtable_latency;
    Cycles cache_snoop_latency;
    std::map<Addr, Tick> LoadsCacheHitRespondingTimeHistory;
    std::map<Addr, Tick> LoadsCacheHitAccessingTimeHistory;
    std::map<Addr, Tick> LoadsMemAccessingTimeHistory;

public:
    MAA *maa;
    IndirectAccessUnit();
    ~IndirectAccessUnit();
    void allocate(int _my_indirect_id,
                  int _num_tile_elements,
                  int _num_row_table_rows_per_bank,
                  int _num_row_table_entries_per_subbank_row,
                  int _num_row_table_config_cache_entries,
                  Cycles _rowtable_latency,
                  Cycles _cache_snoop_latency,
                  int _num_channels,
                  MAA *_maa);
    Status getState() const { return state; }
    bool scheduleNextExecution(bool force = false);
    void scheduleExecuteInstructionEvent(int latency = 0);
    void scheduleSendCachePacketEvent(int latency = 0);
    void scheduleSendCpuPacketEvent(int latency = 0);
    void scheduleSendMemReadPacketEvent(int latency = 0);
    void scheduleSendMemWritePacketEvent(int latency = 0);
    void setInstruction(Instruction *_instruction);
    void unblockMemChannel(int channel);

    bool recvData(const Addr addr,
                  uint8_t *dataptr,
                  bool is_block_cached);

    /* Related to BaseMMU::Translation Inheretance */
    void markDelayed() override {}
    void finish(const Fault &fault, const RequestPtr &req,
                ThreadContext *tc, BaseMMU::Mode mode) override;

protected:
    Instruction *my_instruction;
    std::multiset<IndirectPacket, CompareByTick> my_outstanding_cpu_snoop_pkts;
    std::multiset<IndirectPacket, CompareByTick> my_outstanding_cache_read_pkts;
    std::multiset<IndirectPacket, CompareByTick> my_outstanding_cache_evict_pkts;
    std::multiset<IndirectPacket, CompareByTick> my_outstanding_mem_write_pkts;
    std::multiset<IndirectPacket, CompareByTick> my_outstanding_mem_read_pkts;
    Request::Flags flags = 0;
    const Addr block_size = 64;
    int my_word_size = -1;
    int my_words_per_cl = -1;
    Addr my_virtual_addr = 0;
    Addr my_base_addr;
    int my_dst_tile, my_src_tile, my_cond_tile, my_max, my_idx_tile;
    bool my_cond_tile_ready, my_idx_tile_ready, my_src_tile_ready;
    int my_expected_responses;
    int my_received_responses;
    std::vector<int> my_sorted_indices;
    bool **my_RT_req_sent;
    std::vector<int> *my_RT_bank_order;
    int my_i, my_RT_idx;
    bool my_drain;

    bool my_translation_done;
    Addr my_translated_addr;
    int my_indirect_id;
    Tick my_SPD_read_finish_tick;
    Tick my_SPD_write_finish_tick;
    Tick my_RT_access_finish_tick;
    Tick my_decode_start_tick;
    Tick my_fill_start_tick;
    Tick my_build_start_tick;
    Tick my_request_start_tick;
    std::set<Addr> my_unique_WORD_addrs;
    std::set<Addr> my_unique_CL_addrs;
    std::set<Addr> my_unique_ROW_addrs;

    Addr translatePacket(Addr vaddr);
    bool checkAndResetAllRowTablesSent();
    int getRowTableIdx(int RT_config, int channel, int rank, int bankgroup, int bank);
    Addr getGrowAddr(int RT_config, int bankgroup, int bank, int row);
    int getRowTableConfig(Addr addr);
    void setRowTableConfig(Addr addr, int num_CLs, int num_ROWs);
    void executeInstruction();
    EventFunctionWrapper executeInstructionEvent;
    EventFunctionWrapper sendCachePacketEvent;
    EventFunctionWrapper sendCpuPacketEvent;
    EventFunctionWrapper sendMemReadPacketEvent;
    EventFunctionWrapper sendMemWritePacketEvent;
    void check_reset();
    bool scheduleNextSendCache();
    bool scheduleNextSendCpu();
    bool scheduleNextSendMemRead();
    bool scheduleNextSendMemWrite();
    Cycles updateLatency(int num_spd_read_data_accesses,
                         int num_spd_read_condidx_accesses,
                         int num_spd_write_accesses,
                         int num_rowtable_accesses,
                         int RT_access_parallelism);

public:
    void createCacheReadPacket(Addr addr);
    void createCacheSnoopPacket(Addr addr, int latency);
    void createCacheEvictPacket(Addr addr);
    void createMemReadPacket(Addr addr);
    bool sendOutstandingCachePacket();
    bool sendOutstandingCpuPacket();
    bool sendOutstandingMemReadPacket();
    bool sendOutstandingMemWritePacket();
};
} // namespace gem5

#endif //__MEM_MAA_INDIRECT_ACCESS_HH__
