#ifndef __MEM_MAA_STREAMACCESS_HH__
#define __MEM_MAA_STREAMACCESS_HH__

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

namespace gem5 {

class MAA;

struct RequestTableEntry {
    RequestTableEntry() : itr(0), wid(0) {}
    RequestTableEntry(int _itr, uint16_t _wid) : itr(_itr), wid(_wid) {}
    uint32_t itr;
    uint16_t wid;
};

class RequestTable {
public:
    RequestTable() {
        entries = new RequestTableEntry *[num_addresses];
        entries_valid = new bool *[num_addresses];
        for (int i = 0; i < num_addresses; i++) {
            entries[i] = new RequestTableEntry[num_entries_per_address];
            entries_valid[i] = new bool[num_entries_per_address];
            for (int j = 0; j < num_entries_per_address; j++) {
                entries_valid[i][j] = false;
            }
        }
        addresses = new Addr[num_addresses];
        addresses_valid = new bool[num_addresses];
        for (int i = 0; i < num_addresses; i++) {
            addresses_valid[i] = false;
        }
    }
    ~RequestTable() {
        for (int i = 0; i < num_addresses; i++) {
            delete[] entries[i];
            delete[] entries_valid[i];
        }
        delete[] entries;
        delete[] entries_valid;
        delete[] addresses;
        delete[] addresses_valid;
    }

    bool add_entry(int itr, Addr base_addr, uint16_t wid);
    std::vector<RequestTableEntry> get_entries(Addr base_addr);

protected:
    const int num_addresses = 32;
    const int num_entries_per_address = 16;
    RequestTableEntry **entries;
    bool **entries_valid;
    Addr *addresses;
    bool *addresses_valid;
};

class StreamAccessUnit : public BaseMMU::Translation {
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
    unsigned int num_tile_elements;
    Status state;
    MAA *maa;
    RequestTable *request_table;
    int dst_tile_id;

public:
    StreamAccessUnit();
    ~StreamAccessUnit() {
        if (request_table != nullptr) {
            delete request_table;
        }
    }
    void allocate(int _my_stream_id, unsigned int _num_tile_elements, MAA *_maa) {
        my_stream_id = _my_stream_id;
        num_tile_elements = _num_tile_elements;
        state = Status::Idle;
        maa = _maa;
        dst_tile_id = -1;
        request_table = new RequestTable();
        translation_done = false;
    }
    Status getState() const { return state; }

    void setInstruction(Instruction *_instruction);

    void scheduleExecuteInstructionEvent(int latency = 0);

    bool recvData(const Addr addr,
                  std::vector<uint32_t> data,
                  std::vector<uint16_t> wids);

    /* Related to BaseMMU::Translation Inheretance */
    void markDelayed() override {}
    void finish(const Fault &fault, const RequestPtr &req,
                ThreadContext *tc, BaseMMU::Mode mode) override;

protected:
    Instruction *my_instruction;
    PacketPtr my_pkt;
    bool my_outstanding_pkt;
    Request::Flags flags = 0;
    const Addr block_size = 64;
    const Addr word_size = sizeof(uint32_t);
    int my_i;
    int my_idx;
    Addr my_last_block_addr = 0;
    Addr my_base_addr;
    std::vector<bool> my_byte_enable;
    int my_dst_tile, my_cond_tile, my_min, my_max, my_stride;
    int my_received_responses;
    int my_stream_id;
    bool my_request_table_full;

    Addr my_translated_physical_address;
    bool translation_done;

    void createMyPacket();
    bool sendOutstandingPacket();
    void translatePacket();
    void executeInstruction();
    EventFunctionWrapper executeInstructionEvent;
};
} // namespace gem5

#endif // __MEM_MAA_STREAMACCESS_HH__