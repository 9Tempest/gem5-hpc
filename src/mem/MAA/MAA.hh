#ifndef __MEM_MAA_MAA_HH__
#define __MEM_MAA_MAA_HH__

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <queue>
#include <string>

#include "base/trace.hh"
#include "base/types.hh"
#include "debug/MAAPort.hh"
#include "mem/cache/tags/base.hh"
#include "mem/packet.hh"
#include "mem/packet_queue.hh"
#include "mem/qport.hh"
#include "mem/request.hh"
#include "sim/clocked_object.hh"
#include "sim/system.hh"
#include "arch/generic/mmu.hh"

#define MAA_SPD_TILE_SIZE 1024
#define MAA_SPD_TILE_NUM  4
#define MAA_REG_NUM       32

namespace gem5 {

struct MAAParams;
class MAA;

class TILE {
protected:
    uint32_t *data;
    unsigned int num_tile_elements;
    uint16_t ready;
    uint16_t size;

public:
    uint32_t getData(int element_id) {
        assert((0 <= element_id) && (element_id < num_tile_elements));
        return data[element_id];
    }
    uint8_t *getDataPtr(int element_id) {
        assert((0 <= element_id) && (element_id < num_tile_elements));
        return (uint8_t *)(&data[element_id]);
    }
    void setData(int element_id, uint32_t _data) {
        assert((0 <= element_id) && (element_id < num_tile_elements));
        this->data[element_id] = _data;
    }

    uint16_t getReady() { return ready; }
    void setReady() { this->ready = 1; }
    void unsetReady() { this->ready = 0; }

    uint16_t getSize() { return size; }
    void setSize(uint16_t size) { this->size = size; }

public:
    TILE() { data = nullptr; }
    ~TILE() {
        if (data != nullptr)
            delete[] data;
    }
    void allocate(unsigned int _num_tile_elements) {
        num_tile_elements = _num_tile_elements;
        ready = 1;
        size = 0;
        data = new uint32_t[num_tile_elements];
        memset(data, 0, num_tile_elements * sizeof(uint32_t));
    }
};

class SPD {
protected:
    TILE *tiles;
    unsigned int num_tiles;
    unsigned int num_tile_elements;

public:
    uint32_t getData(int tile_id, int element_id) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        return tiles[tile_id].getData(element_id);
    }
    uint8_t *getDataPtr(int tile_id, int element_id) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        return tiles[tile_id].getDataPtr(element_id);
    }
    void setData(int tile_id, int element_id, uint32_t _data) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        tiles[tile_id].setData(element_id, _data);
    }
    uint16_t getReady(int tile_id) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        return tiles[tile_id].getReady();
    }
    void setReady(int tile_id) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        tiles[tile_id].setReady();
    }
    void unsetReady(int tile_id) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        tiles[tile_id].unsetReady();
    }
    uint16_t getSize(int tile_id) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        return tiles[tile_id].getSize();
    }
    void setSize(int tile_id, uint16_t size) {
        assert((0 <= tile_id) && (tile_id < num_tiles));
        tiles[tile_id].setSize(size);
    }

public:
    SPD(unsigned int _num_tiles, unsigned int _num_tile_elements)
        : num_tiles(_num_tiles),
          num_tile_elements(_num_tile_elements) {
        tiles = new TILE[num_tiles];
        for (int i = 0; i < num_tiles; i++) {
            tiles[i].allocate(num_tile_elements);
        }
    }
    ~SPD() {
        assert(tiles != nullptr);
        delete[] tiles;
    }
};

class RF {
protected:
    uint32_t *data;
    unsigned int num_regs;

public:
    uint32_t getData(int reg_id) {
        assert((0 <= reg_id) && (reg_id < num_regs));
        return data[reg_id];
    }
    uint8_t *getDataPtr(int reg_id) {
        assert((0 <= reg_id) && (reg_id < num_regs));
        return (uint8_t *)(&data[reg_id]);
    }
    void setData(int reg_id, uint32_t _data) {
        assert((0 <= reg_id) && (reg_id < num_regs));
        this->data[reg_id] = _data;
    }

public:
    RF(unsigned int _num_regs) : num_regs(_num_regs) {
        data = new uint32_t[num_regs];
        memset(data, 0, num_regs * sizeof(uint32_t));
    }
    ~RF() {
        assert(data != nullptr);
        delete[] data;
    }
};

class Instruction {
public:
    enum class FuncUnitType : uint8_t {
        ALU = 0,
        STREAM = 1,
        INDIRECT = 2,
        RANGE = 3,
        MAX
    };
    enum class OpcodeType : uint8_t {
        STREAM_LD = 0,
        INDIR_LD = 1,
        INDIR_ST = 2,
        INDIR_RMW = 3,
        RANGE_LOOP = 4,
        CONDITION = 5,
        MAX
    };
    std::string opcode_names[6] = {
        "STREAM_LD",
        "INDIR_LD",
        "INDIR_ST",
        "INDIR_RMW",
        "RANGE_LOOP",
        "CONDITION"};
    enum class OPType : uint8_t {
        ADD_OP = 0,
        SUB_OP = 1,
        MUL_OP = 2,
        DIV_OP = 3,
        MIN_OP = 4,
        MAX_OP = 5,
        GT_OP = 6,
        GTE_OP = 7,
        LT_OP = 8,
        LTE_OP = 9,
        EQ_OP = 10,
        MAX
    };
    std::string optype_names[11] = {
        "ADD",
        "SUB",
        "MUL",
        "DIV",
        "MIN",
        "MAX",
        "GT",
        "GTE",
        "LT",
        "LTE",
        "EQ"};
    enum class DataType : uint8_t {
        INT32_TYPE = 0,
        FLOAT32_TYPE = 1,
        MAX
    };
    std::string datatype_names[2] = {
        "INT32",
        "FLOAT32"};
    enum class Status : uint8_t {
        Idle = 0,
        Service = 1,
        Finish = 2,
        MAX
    };
    std::string status_names[4] = {
        "Idle",
        "Service",
        "Finish",
        "MAX"};
    Addr baseAddr;
    int16_t src1RegID, src2RegID, src3RegID, dst1RegID, dst2RegID;
    int16_t src1SpdID, src2SpdID, dst1SpdID, dst2SpdID;
    int16_t condSpdID;
    // {STREAM_LD, INDIR_LD, INDIR_ST, INDIR_RMW, RANGE_LOOP, CONDITION}
    OpcodeType opcode;
    // {ADD, SUB, MUL, DIV, MIN, MAX, GT, GTE, LT, LTE, EQ}
    OPType optype;
    // {Int, Float}
    DataType datatype;
    // {Idle, Translation, Fill, Request, Response}
    Status state;
    // {ALU, STREAM, INDIRECT}
    FuncUnitType funcUnit;
    ContextID CID;
    Addr PC;
    Instruction()
        : baseAddr(0),
          src1RegID(-1),
          src2RegID(-1),
          src3RegID(-1),
          dst1RegID(-1),
          dst2RegID(-1),
          src1SpdID(-1),
          src2SpdID(-1),
          dst1SpdID(-1),
          dst2SpdID(-1),
          condSpdID(-1),
          opcode(OpcodeType::MAX),
          optype(OPType::MAX),
          datatype(DataType::MAX),
          state(Status::Idle),
          funcUnit(FuncUnitType::MAX),
          CID(-1),
          PC(0) {}

    std::string print() const {
        std::ostringstream str;
        ccprintf(str, "INSTR[%s%s%s%s%s%s%s%s%s%s%s%s%s%s]",
                 "opcode(" + opcode_names[(int)opcode] + ")",
                 optype == OPType::MAX ? "" : " optype(" + optype_names[(int)optype] + ")",
                 " datatype(" + datatype_names[(int)datatype] + ")",
                 " state(" + status_names[(int)state] + ")",
                 src1SpdID == -1 ? "" : " srcSPD1(" + std::to_string(src1SpdID) + ")",
                 src2SpdID == -1 ? "" : " srcSPD2(" + std::to_string(src2SpdID) + ")",
                 src1RegID == -1 ? "" : " srcREG1(" + std::to_string(src1RegID) + ")",
                 src2RegID == -1 ? "" : " srcREG2(" + std::to_string(src2RegID) + ")",
                 src3RegID == -1 ? "" : " srcREG3(" + std::to_string(src3RegID) + ")",
                 dst1SpdID == -1 ? "" : " dstSPD1(" + std::to_string(dst1SpdID) + ")",
                 dst2SpdID == -1 ? "" : " dstSPD2(" + std::to_string(dst2SpdID) + ")",
                 dst1RegID == -1 ? "" : " dstREG1(" + std::to_string(dst1RegID) + ")",
                 dst2RegID == -1 ? "" : " dstREG2(" + std::to_string(dst2RegID) + ")",
                 condSpdID == -1 ? "" : " condSPD(" + std::to_string(condSpdID) + ")");
        return str.str();
    }
};

class IF {
protected:
    Instruction *instructions;
    unsigned int num_instructions;
    bool *valids;
    bool *readies;

public:
    IF(unsigned int _num_instructions) : num_instructions(_num_instructions) {
        instructions = new Instruction[num_instructions];
        valids = new bool[num_instructions];
        readies = new bool[num_instructions];
        for (int i = 0; i < num_instructions; i++) {
            valids[i] = false;
            readies[i] = false;
        }
    }
    ~IF() {
        assert(instructions != nullptr);
        assert(valids != nullptr);
        assert(readies != nullptr);
        delete[] instructions;
        delete[] valids;
        delete[] readies;
    }
    bool pushInstruction(Instruction _instruction, bool ready) {
        bool pushed = false;
        switch (_instruction.opcode) {
        case Instruction::OpcodeType::STREAM_LD: {
            _instruction.funcUnit = Instruction::FuncUnitType::STREAM;
            break;
        }
        case Instruction::OpcodeType::INDIR_LD:
        case Instruction::OpcodeType::INDIR_ST:
        case Instruction::OpcodeType::INDIR_RMW: {
            _instruction.funcUnit = Instruction::FuncUnitType::INDIRECT;
            break;
        }
        case Instruction::OpcodeType::RANGE_LOOP: {
            _instruction.funcUnit = Instruction::FuncUnitType::RANGE;
            break;
        }
        case Instruction::OpcodeType::CONDITION: {
            _instruction.funcUnit = Instruction::FuncUnitType::ALU;
            break;
        }
        default: {
            assert(false);
        }
        }
        for (int i = 0; i < num_instructions; i++) {
            if (valids[i] == false) {
                instructions[i] = _instruction;
                valids[i] = true;
                readies[i] = ready;
                pushed = true;
            } else {
                assert(_instruction.dst1SpdID == -1 || instructions[i].dst1SpdID == -1 || _instruction.dst1SpdID != instructions[i].dst1SpdID);
                assert(_instruction.dst1SpdID == -1 || instructions[i].dst2SpdID == -1 || _instruction.dst1SpdID != instructions[i].dst2SpdID);
                assert(_instruction.dst2SpdID == -1 || instructions[i].dst1SpdID == -1 || _instruction.dst2SpdID != instructions[i].dst1SpdID);
                assert(_instruction.dst2SpdID == -1 || instructions[i].dst2SpdID == -1 || _instruction.dst2SpdID != instructions[i].dst2SpdID);
            }
        }
        return pushed;
    }
    Instruction *getReady(Instruction::FuncUnitType funcUnit) {
        for (int i = 0; i < num_instructions; i++) {
            if (valids[i] &&
                readies[i] &&
                instructions[i].state == Instruction::Status::Idle &&
                instructions[i].funcUnit == funcUnit) {
                instructions[i].state = Instruction::Status::Service;
                return &instructions[i];
            }
        }
        return nullptr;
    }
};

class AddressRangeType {
protected:
    char const *address_range_names[7] = {
        "SPD_DATA_CACHEABLE_RANGE",
        "SPD_DATA_NONCACHEABLE_RANGE",
        "SPD_SIZE_RANGE",
        "SPD_READY_RANGE",
        "SCALAR_RANGE",
        "INSTRUCTION_RANGE",
        "MAX"};

    Addr addr;
    Addr base;
    Addr offset;
    uint8_t rangeID;

public:
    enum class Type : uint8_t {
        SPD_DATA_CACHEABLE_RANGE = 0,
        SPD_DATA_NONCACHEABLE_RANGE = 1,
        SPD_SIZE_RANGE = 2,
        SPD_READY_RANGE = 3,
        SCALAR_RANGE = 4,
        INSTRUCTION_RANGE = 5,
        MAX
    };
    AddressRangeType(Addr _addr, AddrRangeList addrRanges) : addr(_addr) {
        bool range_found = false;
        rangeID = 0;
        for (const auto &r : addrRanges) {
            if (r.contains(addr)) {
                base = r.start();
                offset = addr - base;
                range_found = true;
                break;
            }
            rangeID++;
        }
        assert(range_found);
    }
    std::string print() const {
        std::ostringstream str;
        ccprintf(str, "%s: 0x%lx + 0x%lx", address_range_names[rangeID], base, offset);
        return str.str();
    }
    Type getType() const { return static_cast<Type>(rangeID); }
    Addr getOffset() const { return offset; }
};

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
    StreamAccessUnit() {
        request_table = nullptr;
    }
    ~StreamAccessUnit() {
        if (request_table != nullptr) {
            delete request_table;
        }
    }
    void allocate(unsigned int _num_tile_elements, MAA *_maa) {
        num_tile_elements = _num_tile_elements;
        state = Status::Idle;
        maa = _maa;
        dst_tile_id = -1;
        request_table = new RequestTable();
        translation_done = false;
    }
    Status getState() const { return state; }
    void execute(Instruction *_instruction = nullptr);
    void recvData(const Addr addr,
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

    Addr my_translated_physical_address;
    bool translation_done;

    void createMyPacket();
    bool sendOutstandingPacket();
    void translatePacket();
};

/**
 * A basic cache interface. Implements some common functions for speed.
 */
class MAA : public ClockedObject {
    /**
     * A cache response port is used for the CPU-side port of the cache,
     * and it is basically a simple timing port that uses a transmit
     * list for responses to the CPU (or connected requestor). In
     * addition, it has the functionality to block the port for
     * incoming requests. If blocked, the port will issue a retry once
     * unblocked.
     */
    class MAAResponsePort : public QueuedResponsePort {

    protected:
        MAAResponsePort(const std::string &_name, MAA &_maa, const std::string &_label);

        MAA &maa;

        /** A normal packet queue used to store responses. */
        RespPacketQueue queue;
    };

    /**
     * The CPU-side port extends the base MAA response port with access
     * functions for functional, atomic and timing requests.
     */
    class CpuSidePort : public MAAResponsePort {
    protected:
        bool recvTimingSnoopResp(PacketPtr pkt) override;

        bool tryTiming(PacketPtr pkt) override;

        bool recvTimingReq(PacketPtr pkt) override;

        Tick recvAtomic(PacketPtr pkt) override;

        void recvFunctional(PacketPtr pkt) override;

        AddrRangeList getAddrRanges() const override;

    public:
        CpuSidePort(const std::string &_name, MAA &_maa,
                    const std::string &_label);
    };

    class MAARequestPort : public QueuedRequestPort {
    public:
        /**
         * Schedule a send of a request packet (from the MSHR). Note
         * that we could already have a retry outstanding.
         */
        void schedSendEvent(Tick time) {
            reqQueue.schedSendEvent(time);
        }

    protected:
        MAARequestPort(const std::string &_name,
                       ReqPacketQueue &_reqQueue,
                       SnoopRespPacketQueue &_snoopRespQueue)
            : QueuedRequestPort(_name, _reqQueue, _snoopRespQueue) {}

        /**
         * Memory-side port never snoops.
         *
         * @return always false
         */
        bool isSnooping() const { return false; }
    };

    /**
     * Override the default behaviour of sendDeferredPacket to enable
     * the memory-side cache port to also send requests based on the
     * current MSHR status. This queue has a pointer to our specific
     * cache implementation and is used by the MemSidePort.
     */
    class MAAReqPacketQueue : public ReqPacketQueue {

    protected:
        MAA &maa;
        SnoopRespPacketQueue &snoopRespQueue;

    public:
        MAAReqPacketQueue(MAA &maa, RequestPort &port,
                          SnoopRespPacketQueue &snoop_resp_queue,
                          const std::string &label) : ReqPacketQueue(maa, port, label), maa(maa),
                                                      snoopRespQueue(snoop_resp_queue) {}

        /**
         * Override the normal sendDeferredPacket and do not only
         * consider the transmit list (used for responses), but also
         * requests.
         */
        void sendDeferredPacket();
    };

    /**
     * The memory-side port extends the base cache request port with
     * access functions for functional, atomic and timing snoops.
     */
    class MemSidePort : public MAARequestPort {
    private:
        /** The maa-specific queue. */
        MAAReqPacketQueue _reqQueue;

        SnoopRespPacketQueue _snoopRespQueue;

        // a pointer to our specific cache implementation
        MAA *maa;

    protected:
        void recvTimingSnoopReq(PacketPtr pkt);

        bool recvTimingResp(PacketPtr pkt);

        Tick recvAtomicSnoop(PacketPtr pkt);

        void recvFunctionalSnoop(PacketPtr pkt);

        void recvReqRetry();

    public:
        MemSidePort(const std::string &_name, MAA *_maa,
                    const std::string &_label);
    };

public:
    CpuSidePort cpuSidePort;
    MemSidePort memSidePort;
    SPD *spd;
    RF *rf;
    IF *ifile;
    StreamAccessUnit *streamAccessUnits;

protected:
    /**
     * Performs the access specified by the request.
     * @param pkt The request to perform.
     */
    void recvTimingReq(PacketPtr pkt);

    /**
     * Handles a response (cache line fill/write ack) from the bus.
     * @param pkt The response packet
     */
    void recvTimingResp(PacketPtr pkt);

    /**
     * Handle a snoop response.
     * @param pkt Snoop response packet
     */
    void recvTimingSnoopResp(PacketPtr pkt);

    /**
     * Performs the access specified by the request.
     * @param pkt The request to perform.
     * @return The number of ticks required for the access.
     */
    Tick recvAtomic(PacketPtr pkt);

    /**
     * Snoop for the provided request in the cache and return the estimated
     * time taken.
     * @param pkt The memory request to snoop
     * @return The number of ticks required for the snoop.
     */
    Tick recvAtomicSnoop(PacketPtr pkt);

    /**
     * Performs the access specified by the request.
     *
     * @param pkt The request to perform.
     * @param fromCpuSide from the CPU side port or the memory side port
     */
    void functionalAccess(PacketPtr pkt, bool from_cpu_side);

    /**
     * Determine if an address is in the ranges covered by this
     * cache. This is useful to filter snoops.
     *
     * @param addr Address to check against
     *
     * @return The id of the range that contains the address, or -1 if none
     */
    int inRange(Addr addr) const;

    /**
     * Snoops bus transactions to maintain coherence.
     * @param pkt The current bus transaction.
     */
    void recvTimingSnoopReq(PacketPtr pkt);

    /**
     * The address range to which the cache responds on the CPU side.
     * Normally this is all possible memory addresses. */
    const AddrRangeList addrRanges;

public:
    unsigned int num_tiles;
    unsigned int num_tile_elements;
    unsigned int num_regs;
    unsigned int num_instructions;
    unsigned int num_stream_access_units;
    unsigned int num_indirect_access_units;
    unsigned int num_range_units;
    unsigned int num_alu_units;
    Instruction current_instruction;
    RequestorID requestorId;

public:
    /** System we are currently operating in. */
    System *system;

    /** Registered mmu for address translations */
    BaseMMU *mmu;

public:
    MAA(const MAAParams &p);
    ~MAA();

    void init() override;

    Port &getPort(const std::string &if_name,
                  PortID idx = InvalidPortID) override;

    const AddrRangeList &getAddrRanges() const { return addrRanges; }

protected:
    void issueInstruction();
};

} // namespace gem5

#endif //__MEM_MAA_MAA_HH__
