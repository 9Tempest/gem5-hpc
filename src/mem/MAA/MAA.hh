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
#include "mem/cache/tags/base.hh"
#include "mem/packet.hh"
#include "mem/packet_queue.hh"
#include "mem/qport.hh"
#include "mem/request.hh"
#include "mem/ramulator2.hh"
#include "sim/clocked_object.hh"
#include "sim/system.hh"
#include "arch/generic/mmu.hh"

namespace gem5 {

struct MAAParams;
class IF;
class RF;
class SPD;
class StreamAccessUnit;
class IndirectAccessUnit;
class Invalidator;
class ALUUnit;
class RangeFuserUnit;
class Instruction;

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
        enum class BlockReason : uint8_t {
            NOT_BLOCKED,
            MAX_XBAR_PACKETS
        };

    protected:
        bool recvTimingSnoopResp(PacketPtr pkt) override;

        bool tryTiming(PacketPtr pkt) override;

        bool recvTimingReq(PacketPtr pkt) override;

        Tick recvAtomic(PacketPtr pkt) override;

        void recvFunctional(PacketPtr pkt) override;

        AddrRangeList getAddrRanges() const override;

    protected:
        int outstandingCpuSidePackets;
        int maxOutstandingCpuSidePackets;
        BlockReason blockReason;
        BlockReason *funcBlockReasons[3];
        void setUnblocked();

    public:
        bool sendSnoopPacket(uint8_t func_unit_type,
                             int func_unit_id,
                             PacketPtr pkt);
        void allocate(int _maxOutstandingCpuSidePackets);

    public:
        CpuSidePort(const std::string &_name, MAA &_maa,
                    const std::string &_label);
    };

    class MAAMemRequestPort : public QueuedRequestPort {
    public:
        /**
         * Schedule a send of a request packet (from the MSHR). Note
         * that we could already have a retry outstanding.
         */
        void schedSendEvent(Tick time) {
            reqQueue.schedSendEvent(time);
        }

    protected:
        MAAMemRequestPort(const std::string &_name,
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

    class MAACacheRequestPort : public QueuedRequestPort {
    public:
        /**
         * Schedule a send of a request packet (from the MSHR). Note
         * that we could already have a retry outstanding.
         */
        void schedSendEvent(Tick time) {
            reqQueue.schedSendEvent(time);
        }

    protected:
        MAACacheRequestPort(const std::string &_name,
                            ReqPacketQueue &_reqQueue,
                            SnoopRespPacketQueue &_snoopRespQueue)
            : QueuedRequestPort(_name, _reqQueue, _snoopRespQueue) {}

        /**
         * Memory-side port always snoops.
         *
         * @return always true
         */
        bool isSnooping() const { return true; }
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
    class MemSidePort : public MAAMemRequestPort {
        enum class BlockReason : uint8_t {
            NOT_BLOCKED,
            MEM_FAILED
        };

    private:
        /** The maa-specific queue. */
        MAAReqPacketQueue _reqQueue;

        SnoopRespPacketQueue _snoopRespQueue;

        // a pointer to our specific MAA implementation
        MAA *maa;

    protected:
        void recvTimingSnoopReq(PacketPtr pkt);

        bool recvTimingResp(PacketPtr pkt);

        Tick recvAtomicSnoop(PacketPtr pkt);

        void recvFunctionalSnoop(PacketPtr pkt);

        void recvReqRetry();

    protected:
        BlockReason blockReason;
        BlockReason *funcBlockReasons;
        void setUnblocked(BlockReason reason);

    public:
        bool sendPacket(int func_unit_id,
                        PacketPtr pkt);
        void allocate();

    public:
        MemSidePort(const std::string &_name, MAA *_maa,
                    const std::string &_label);
    };

    /**
     * The memory-side port extends the base cache request port with
     * access functions for functional, atomic and timing snoops.
     */
    class CacheSidePort : public MAACacheRequestPort {
        enum class BlockReason : uint8_t {
            NOT_BLOCKED,
            MAX_XBAR_PACKETS,
            CACHE_FAILED
        };

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

    protected:
        int outstandingCacheSidePackets;
        int maxOutstandingCacheSidePackets;
        BlockReason blockReason;
        BlockReason *funcBlockReasons[3];
        void setUnblocked(BlockReason reason);

    public:
        bool sendPacket(uint8_t func_unit_type,
                        int func_unit_id,
                        PacketPtr pkt);
        void allocate(int _maxOutstandingCacheSidePackets);

    public:
        CacheSidePort(const std::string &_name, MAA *_maa,
                      const std::string &_label);
    };

public:
    CpuSidePort cpuSidePort;
    MemSidePort memSidePort;
    CacheSidePort cacheSidePort;
    SPD *spd;
    RF *rf;
    IF *ifile;
    StreamAccessUnit *streamAccessUnits;
    IndirectAccessUnit *indirectAccessUnits;
    Invalidator *invalidator;
    ALUUnit *aluUnits;
    RangeFuserUnit *rangeUnits;

    // Ramulator related variables for address mapping
    std::vector<int> m_org;
    std::vector<int> m_addr_bits; // How many address bits for each level in the hierarchy?
    int m_num_levels;             // How many levels in the hierarchy?
    int m_tx_offset;
    int m_col_bits_idx;
    int m_row_bits_idx;

public:
    std::vector<int> map_addr(Addr addr);
    Addr calc_Grow_addr(std::vector<int> addr_vec);
    void addRamulator(memory::Ramulator2 *_ramulator2);

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
    void recvMemTimingResp(PacketPtr pkt);

    /**
     * Handles a response (cache line fill/write ack) from the bus.
     * @param pkt The response packet
     */
    void recvCacheTimingResp(PacketPtr pkt);

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
    Tick recvMemAtomicSnoop(PacketPtr pkt);

    /**
     * Snoop for the provided request in the cache and return the estimated
     * time taken.
     * @param pkt The memory request to snoop
     * @return The number of ticks required for the snoop.
     */
    Tick recvCacheAtomicSnoop(PacketPtr pkt);

    /**
     * Performs the access specified by the request.
     *
     * @param pkt The request to perform.
     * @param fromCpuSide from the CPU side port or the memory side port
     */
    void memFunctionalAccess(PacketPtr pkt, bool from_cpu_side);

    /**
     * Performs the access specified by the request.
     *
     * @param pkt The request to perform.
     * @param fromCpuSide from the CPU side port or the memory side port
     */
    void cacheFunctionalAccess(PacketPtr pkt, bool from_cpu_side);

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
    void recvMemTimingSnoopReq(PacketPtr pkt);

    /**
     * Snoops bus transactions to maintain coherence.
     * @param pkt The current bus transaction.
     */
    void recvCacheTimingSnoopReq(PacketPtr pkt);

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
    unsigned int num_row_table_rows;
    unsigned int num_row_table_entries_per_row;
    Cycles rowtable_latency;
    Cycles cache_snoop_latency;
    Instruction *current_instruction;
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
    void setTileReady(int tileID);
    void finishInstruction(Instruction *instruction,
                           int dst1SpdID = -1,
                           int dst2SpdID = -1);
    void setTileInvalidated(Instruction *instruction, int tileID);
    bool sentMemSidePacket(PacketPtr pkt);
    Tick getClockEdge(Cycles cycles = Cycles(0)) const;
    Cycles getTicksToCycles(Tick t) const;
    Tick getCyclesToTicks(Cycles c) const;
    void resetStats() override;

protected:
    PacketPtr my_instruction_pkt;
    PacketPtr my_ready_pkt;
    bool my_outstanding_instruction_pkt;
    bool my_outstanding_ready_pkt;
    Tick my_last_idle_tick;
    int my_ready_tile_id;
    void issueInstruction();
    void dispatchInstruction();
    EventFunctionWrapper issueInstructionEvent, dispatchInstructionEvent;
    void scheduleIssueInstructionEvent(int latency = 0);
    void scheduleDispatchInstructionEvent(int latency = 0);
    bool allFuncUnitsIdle();

public:
    struct MAAStats : public statistics::Group {
        MAAStats(statistics::Group *parent,
                 int num_indirect_access_units,
                 int num_stream_access_units,
                 int num_range_units,
                 int num_alu_units);

        /** Number of instructions. */
        statistics::Scalar numInst_INDRD;
        statistics::Scalar numInst_INDWR;
        statistics::Scalar numInst_INDRMW;
        statistics::Scalar numInst_STRRD;
        statistics::Scalar numInst_RANGE;
        statistics::Scalar numInst_ALUS;
        statistics::Scalar numInst_ALUV;
        statistics::Scalar numInst_INV;
        statistics::Scalar numInst;

        /** Cycles of instructions. */
        statistics::Scalar cycles_INDRD;
        statistics::Scalar cycles_INDWR;
        statistics::Scalar cycles_INDRMW;
        statistics::Scalar cycles_STRRD;
        statistics::Scalar cycles_RANGE;
        statistics::Scalar cycles_ALUS;
        statistics::Scalar cycles_ALUV;
        statistics::Scalar cycles_INV;
        statistics::Scalar cycles_IDLE;
        statistics::Scalar cycles;

        /** Average cycles per instruction. */
        statistics::Formula avgCPI_INDRD;
        statistics::Formula avgCPI_INDWR;
        statistics::Formula avgCPI_INDRMW;
        statistics::Formula avgCPI_STRRD;
        statistics::Formula avgCPI_RANGE;
        statistics::Formula avgCPI_ALUS;
        statistics::Formula avgCPI_ALUV;
        statistics::Formula avgCPI_INV;
        statistics::Formula avgCPI;

        /** Indirect Unit -- Row-Table Statistics. */
        std::vector<statistics::Scalar *> IND_NumInsts;
        std::vector<statistics::Scalar *> IND_NumWordsInserted;
        std::vector<statistics::Scalar *> IND_NumCacheLineInserted;
        std::vector<statistics::Scalar *> IND_NumRowsInserted;
        std::vector<statistics::Scalar *> IND_NumDrains;
        std::vector<statistics::Formula *> IND_AvgWordsPerCacheLine;
        std::vector<statistics::Formula *> IND_AvgCacheLinesPerRow;
        std::vector<statistics::Formula *> IND_AvgRowsPerInst;
        std::vector<statistics::Formula *> IND_AvgDrainsPerInst;

        /** Indirect Unit -- Cycles of stages. */
        std::vector<statistics::Scalar *> IND_CyclesFill;
        std::vector<statistics::Scalar *> IND_CyclesDrain;
        std::vector<statistics::Scalar *> IND_CyclesBuild;
        std::vector<statistics::Scalar *> IND_CyclesRequest;
        std::vector<statistics::Scalar *> IND_CyclesRTAccess;
        std::vector<statistics::Scalar *> IND_CyclesSPDReadAccess;
        std::vector<statistics::Scalar *> IND_CyclesSPDWriteAccess;
        std::vector<statistics::Formula *> IND_AvgCyclesFillPerInst;
        std::vector<statistics::Formula *> IND_AvgCyclesDrainPerInst;
        std::vector<statistics::Formula *> IND_AvgCyclesBuildPerInst;
        std::vector<statistics::Formula *> IND_AvgCyclesRequestPerInst;
        std::vector<statistics::Formula *> IND_AvgCyclesRTAccessPerInst;
        std::vector<statistics::Formula *> IND_AvgCyclesSPDReadAccessPerInst;
        std::vector<statistics::Formula *> IND_AvgCyclesSPDWriteAccessPerInst;

        /** Indirect Unit -- Load accesses. */
        std::vector<statistics::Scalar *> IND_LoadsCacheHitResponding;
        std::vector<statistics::Scalar *> IND_LoadsCacheHitAccessing;
        std::vector<statistics::Scalar *> IND_LoadsMemAccessing;
        std::vector<statistics::Formula *> IND_AvgLoadsCacheHitRespondingPerInst;
        std::vector<statistics::Formula *> IND_AvgLoadsCacheHitAccessingPerInst;
        std::vector<statistics::Formula *> IND_AvgLoadsMemAccessingPerInst;

        /** Indirect Unit -- Store accesses. */
        std::vector<statistics::Scalar *> IND_StoresMemAccessing;
        std::vector<statistics::Formula *> IND_AvgStoresMemAccessingPerInst;

        /** Indirect Unit -- Evict accesses. */
        std::vector<statistics::Scalar *> IND_Evicts;
        std::vector<statistics::Formula *> IND_AvgEvictssPerInst;

        /** Stream Unit -- Row-Table Statistics. */
        std::vector<statistics::Scalar *> STR_NumInsts;
        std::vector<statistics::Scalar *> STR_NumWordsInserted;
        std::vector<statistics::Scalar *> STR_NumCacheLineInserted;
        std::vector<statistics::Scalar *> STR_NumDrains;
        std::vector<statistics::Formula *> STR_AvgWordsPerCacheLine;
        std::vector<statistics::Formula *> STR_AvgCacheLinesPerInst;
        std::vector<statistics::Formula *> STR_AvgDrainsPerInst;

        /** Stream Unit -- Cycles of stages. */
        std::vector<statistics::Scalar *> STR_CyclesRequest;
        std::vector<statistics::Scalar *> STR_CyclesRTAccess;
        std::vector<statistics::Scalar *> STR_CyclesSPDReadAccess;
        std::vector<statistics::Scalar *> STR_CyclesSPDWriteAccess;
        std::vector<statistics::Formula *> STR_AvgCyclesRequestPerInst;
        std::vector<statistics::Formula *> STR_AvgCyclesRTAccessPerInst;
        std::vector<statistics::Formula *> STR_AvgCyclesSPDReadAccessPerInst;
        std::vector<statistics::Formula *> STR_AvgCyclesSPDWriteAccessPerInst;

        /** Stream Unit -- Load accesses. */
        std::vector<statistics::Scalar *> STR_LoadsCacheAccessing;
        std::vector<statistics::Formula *> STR_AvgLoadsCacheAccessingPerInst;

        /** Stream Unit -- Evict accesses. */
        std::vector<statistics::Scalar *> STR_Evicts;
        std::vector<statistics::Formula *> STR_AvgEvictssPerInst;

        /** Range Fuser Unit -- Cycles of stages. */
        std::vector<statistics::Scalar *> RNG_NumInsts;
        std::vector<statistics::Scalar *> RNG_CyclesCompute;
        std::vector<statistics::Scalar *> RNG_CyclesSPDReadAccess;
        std::vector<statistics::Scalar *> RNG_CyclesSPDWriteAccess;
        std::vector<statistics::Formula *> RNG_AvgCyclesComputePerInst;
        std::vector<statistics::Formula *> RNG_AvgCyclesSPDReadAccessPerInst;
        std::vector<statistics::Formula *> RNG_AvgCyclesSPDWriteAccessPerInst;

        /** ALU Unit -- Cycles of stages. */
        std::vector<statistics::Scalar *> ALU_NumInsts;
        std::vector<statistics::Scalar *> ALU_NumInstsCompare;
        std::vector<statistics::Scalar *> ALU_NumInstsCompute;
        std::vector<statistics::Scalar *> ALU_CyclesCompute;
        std::vector<statistics::Scalar *> ALU_CyclesSPDReadAccess;
        std::vector<statistics::Scalar *> ALU_CyclesSPDWriteAccess;
        std::vector<statistics::Formula *> ALU_AvgCyclesComputePerInst;
        std::vector<statistics::Formula *> ALU_AvgCyclesSPDReadAccessPerInst;
        std::vector<statistics::Formula *> ALU_AvgCyclesSPDWriteAccessPerInst;

        /** ALU Unit -- Comparison Info. */
        std::vector<statistics::Scalar *> ALU_NumComparedWords;
        std::vector<statistics::Scalar *> ALU_NumTakenWords;
        std::vector<statistics::Formula *> ALU_AvgNumTakenWordsPerComparedWords;

        /** ALU Unit -- Comparison Info. */
        statistics::Scalar *INV_NumInvalidatedCachelines;
        statistics::Formula *INV_AvgInvalidatedCachelinesPerInst;

    } stats;
};
/**
 * Returns the address of the closest aligned fixed-size block to the given
 * address.
 * @param addr Input address.
 * @param block_size Block size in bytes.
 * @return Address of the closest aligned block.
 */
inline Addr addrBlockAlign(Addr addr, Addr block_size) {
    return addr & ~(block_size - 1);
}
} // namespace gem5

#endif //__MEM_MAA_MAA_HH__
