#ifndef __MEM_MAA_INDIRECT_ACCESS_HH__
#define __MEM_MAA_INDIRECT_ACCESS_HH__

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>

#include "base/types.hh"
#include "mem/packet.hh"
#include "mem/request.hh"
#include "sim/system.hh"
#include "arch/generic/mmu.hh"
#include "mem/MAA/IF.hh"

#define ADDR_CHANNEL_LEVEL   0
#define ADDR_RANK_LEVEL      1
#define ADDR_BANKGROUP_LEVEL 2
#define ADDR_BANK_LEVEL      3
#define ADDR_ROW_LEVEL       4
#define ADDR_COLUMN_LEVEL    5

namespace gem5 {

class MAA;
class IndirectAccessUnit;

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
    void allocate(int _num_tile_elements);
    void insert(int itr, int wid, int last_itr);
    std::vector<OffsetTableEntry> get_entry_recv(int first_itr);
    void reset();
    OffsetTableEntry *entries;
    bool *entries_valid;
    int num_tile_elements;
};

class RowTableEntry {
public:
    struct Entry {
        Addr addr;
        int first_itr;
        int last_itr;
        bool is_cached;
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
    bool insert(Addr addr,
                int itr,
                int wid);
    void reset();
    bool get_entry_send(Addr &addr, bool &is_block_cached);
    std::vector<OffsetTableEntry> get_entry_recv(Addr addr,
                                                 bool is_block_cached);
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
    void insert(Addr Grow_addr,
                Addr addr,
                int itr,
                int wid);
    bool get_entry_send(Addr &addr,
                        bool &is_block_cached);
    std::vector<OffsetTableEntry> get_entry_recv(Addr Grow_addr,
                                                 Addr addr,
                                                 bool is_block_cached);

    void reset();
    OffsetTable *offset_table;
    RowTableEntry *entries;
    bool *entries_valid;
    bool *entries_received;
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
        Request = 2,
        Response = 3,
        max
    };

protected:
    std::string status_names[5] = {
        "Idle",
        "Decode",
        "Request",
        "Response",
        "max"};
    int num_row_table_banks;
    int num_tile_elements;
    int num_row_table_rows;
    int num_row_table_entries_per_row;
    Status state;
    MAA *maa;
    RowTable *row_table;
    OffsetTable *offset_table;
    int dst_tile_id;

public:
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
                  MAA *_maa);
    Status getState() const { return state; }
    void scheduleExecuteInstructionEvent(int latency = 0);
    void setInstruction(Instruction *_instruction);

    bool recvData(const Addr addr,
                  std::vector<uint32_t> data,
                  std::vector<uint16_t> wids,
                  bool is_block_cached);

    /* Related to BaseMMU::Translation Inheretance */
    void markDelayed() override {}
    void finish(const Fault &fault, const RequestPtr &req,
                ThreadContext *tc, BaseMMU::Mode mode) override;
    bool checkBlockCached(Addr physical_addr);

protected:
    Instruction *my_instruction;
    PacketPtr my_pkt;
    bool my_outstanding_pkt;
    Request::Flags flags = 0;
    const Addr block_size = 64;
    const Addr word_size = sizeof(uint32_t);
    Addr my_virtual_addr = 0;
    Addr my_base_addr;
    int my_dst_tile, my_cond_tile, my_max, my_idx_tile;
    int my_received_responses;
    std::vector<int> my_sorted_indices;
    bool *my_row_table_req_sent;
    bool my_all_row_table_req_sent;
    bool my_is_block_cached;
    int my_last_row_table_sent;
    std::vector<int> my_row_table_bank_order;

    Addr my_translated_physical_address;
    Addr my_translated_block_physical_address;
    bool translation_done;
    int my_indirect_id;

    void createMyPacket();
    bool sendOutstandingPacket();
    void translatePacket();
    void checkAllRowTablesSent();
    int getRowTableIdx(int channel, int rank, int bankgroup);
    void executeInstruction();
    EventFunctionWrapper executeInstructionEvent;
};
} // namespace gem5

#endif //__MEM_MAA_INDIRECT_ACCESS_HH__
