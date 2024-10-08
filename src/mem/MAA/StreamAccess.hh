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
class StreamAccessUnit;

struct RequestTableEntry {
    RequestTableEntry() : itr(0), wid(0) {}
    RequestTableEntry(int _itr, uint16_t _wid) : itr(_itr), wid(_wid) {}
    uint32_t itr;
    uint16_t wid;
};

class RequestTable {
public:
    RequestTable(StreamAccessUnit *_stream_access, int _my_stream_id);
    ~RequestTable();

    bool add_entry(int itr, Addr base_addr, uint16_t wid);
    bool is_full();
    std::vector<RequestTableEntry> get_entries(Addr base_addr);
    void check_reset();
    void reset();

protected:
    const int num_addresses = 32;
    const int num_entries_per_address = 16;
    RequestTableEntry **entries;
    bool **entries_valid;
    Addr *addresses;
    bool *addresses_valid;
    StreamAccessUnit *stream_access;
    int my_stream_id;
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
    class StreamPacket {
    public:
        PacketPtr packet;
        Tick tick;
        StreamPacket(PacketPtr _packet, Tick _tick)
            : packet(_packet), tick(_tick) {}
        StreamPacket(const StreamPacket &other) {
            packet = other.packet;
            tick = other.tick;
        }
        bool operator<(const StreamPacket &rhs) const {
            return tick < rhs.tick;
        }
    };
    struct CompareByTick {
        bool operator()(const StreamPacket &lhs, const StreamPacket &rhs) const {
            return lhs.tick < rhs.tick;
        }
    };
    std::multiset<StreamPacket, CompareByTick> my_outstanding_read_pkts;
    unsigned int num_tile_elements;
    Status state;
    RequestTable *request_table;
    int dst_tile_id;

public:
    StreamAccessUnit();
    ~StreamAccessUnit() {
        if (request_table != nullptr) {
            delete request_table;
        }
    }
    void allocate(int _my_stream_id, unsigned int _num_tile_elements, MAA *_maa);

    Status getState() const { return state; }

    void setInstruction(Instruction *_instruction);

    Cycles updateLatency(int num_spd_read_accesses,
                         int num_spd_write_accesses,
                         int num_requesttable_accesses);
    bool scheduleNextExecution(bool force = false);
    void scheduleExecuteInstructionEvent(int latency = 0);
    void scheduleSendPacketEvent(int latency = 0);
    bool recvData(const Addr addr,
                  uint8_t *dataptr);

    /* Related to BaseMMU::Translation Inheretance */
    void markDelayed() override {}
    void finish(const Fault &fault, const RequestPtr &req,
                ThreadContext *tc, BaseMMU::Mode mode) override;
    MAA *maa;

protected:
    Instruction *my_instruction;
    Request::Flags flags = 0;
    const Addr block_size = 64;
    int my_i;
    int my_idx;
    Addr my_base_addr, my_last_block_vaddr;
    int my_dst_tile, my_cond_tile, my_min, my_max, my_stride;
    int my_received_responses, my_sent_requests;
    int my_stream_id;
    Tick my_SPD_read_finish_tick;
    Tick my_SPD_write_finish_tick;
    Tick my_RT_access_finish_tick;
    int my_word_size;
    int my_words_per_cl;
    Tick my_decode_start_tick;
    Tick my_request_start_tick;

    Addr my_translated_addr;
    bool my_translation_done;

    void createReadPacket(Addr addr, int latency);
    void createReadPacketEvict(Addr addr);
    bool sendOutstandingReadPacket();
    Addr translatePacket(Addr vaddr);
    void executeInstruction();
    EventFunctionWrapper executeInstructionEvent;
    EventFunctionWrapper sendPacketEvent;
    bool scheduleNextSend();
};
} // namespace gem5

#endif // __MEM_MAA_STREAMACCESS_HH__