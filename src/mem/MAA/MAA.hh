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
#include "mem/ramulator2.hh"
#include "sim/clocked_object.hh"
#include "sim/system.hh"
#include "arch/generic/mmu.hh"
#include "mem/MAA/IF.hh"
#include "mem/MAA/SPD.hh"
#include "mem/MAA/StreamAccess.hh"
#include "mem/MAA/IndirectAccess.hh"
#include "mem/MAA/Invalidator.hh"
#include "mem/MAA/ALU.hh"
#include "mem/MAA/RangeFuser.hh"

namespace gem5 {

struct MAAParams;

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
    class MemSidePort : public MAAMemRequestPort {
        enum class BlockReason : uint8_t {
            NOT_BLOCKED,
            MEM_FAILED
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
        bool sendPacket(FuncUnitType func_unit_type,
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
    void finishInstruction(Instruction *instruction,
                           int dst1SpdID = -1,
                           int dst2SpdID = -1);
    void setDstReady(Instruction *instruction, int dstSpdID);
    bool sentMemSidePacket(PacketPtr pkt);

protected:
    PacketPtr my_instruction_pkt;
    bool my_outstanding_instruction_pkt;
    void issueInstruction();
    void dispatchInstruction();
    EventFunctionWrapper issueInstructionEvent, dispatchInstructionEvent;
    void scheduleIssueInstructionEvent(int latency = 0);
    void scheduleDispatchInstructionEvent(int latency = 0);
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
