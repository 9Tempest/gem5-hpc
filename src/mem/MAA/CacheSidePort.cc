#include "mem/MAA/ALU.hh"
#include "mem/MAA/IF.hh"
#include "mem/MAA/IndirectAccess.hh"
#include "mem/MAA/Invalidator.hh"
#include "mem/MAA/RangeFuser.hh"
#include "mem/MAA/SPD.hh"
#include "mem/MAA/StreamAccess.hh"
#include "mem/MAA/MAA.hh"

#include "base/addr_range.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "mem/packet.hh"
#include "params/MAA.hh"
#include "debug/MAA.hh"
#include "debug/MAACpuPort.hh"
#include "debug/MAACachePort.hh"
#include "debug/MAAMemPort.hh"
#include "debug/MAAController.hh"
#include "sim/cur_tick.hh"
#include <cassert>
#include <cstdint>

#ifndef TRACING_ON
#define TRACING_ON 1
#endif

namespace gem5 {

void MAA::recvCacheTimingResp(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACachePort, "%s: received %s, cmd: %s, size: %d\n",
            __func__,
            pkt->print(),
            pkt->cmdString(),
            pkt->getSize());
    // for (int i = 0; i < pkt->getSize(); i++) {
    //     DPRINTF(MAACachePort, "%02x %s\n", pkt->getPtr<uint8_t>()[i], pkt->req->getByteEnable()[i] ? "True" : "False");
    // }
    switch (pkt->cmd.toInt()) {
    case MemCmd::ReadExResp:
    case MemCmd::ReadResp: {
        assert(pkt->getSize() == 64);
        std::vector<uint32_t> data;
        std::vector<uint16_t> wid;
        for (int i = 0; i < 64; i += 4) {
            if (pkt->req->getByteEnable()[i] == true) {
                data.push_back(*(pkt->getPtr<uint32_t>() + i / 4));
                wid.push_back(i / 4);
            }
        }
        bool received = false;
        if (pkt->cmd == MemCmd::ReadResp) {
            for (int i = 0; i < num_stream_access_units; i++) {
                if (streamAccessUnits[i].getState() == StreamAccessUnit::Status::Request) {
                    if (streamAccessUnits[i].recvData(pkt->getAddr(), pkt->getPtr<uint8_t>())) {
                        panic_if(received, "Received multiple responses for the same request\n");
                        received = true;
                    }
                }
            }
        }
        if (received == false) {
            for (int i = 0; i < num_indirect_access_units; i++) {
                if (indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Request) {
                    if (indirectAccessUnits[i].recvData(pkt->getAddr(), pkt->getPtr<uint8_t>(), true)) {
                        panic_if(received, "Received multiple responses for the same request\n");
                    }
                }
            }
        }
        break;
    }
    case MemCmd::InvalidateResp: {
        assert(false);
        // assert(pkt->getSize() == 64);
        // AddressRangeType address_range = AddressRangeType(pkt->getAddr(), addrRanges);
        // assert(address_range.getType() == AddressRangeType::Type::SPD_DATA_CACHEABLE_RANGE);
        // Addr offset = address_range.getOffset();
        // int tile_id = offset / (num_tile_elements * sizeof(uint32_t));
        // int element_id = offset % (num_tile_elements * sizeof(uint32_t));
        // assert(element_id % sizeof(uint32_t) == 0);
        // element_id /= sizeof(uint32_t);
        // invalidator->recvData(tile_id, element_id);
        break;
    }
    default:
        assert(false);
    }
}
bool MAA::CacheSidePort::recvTimingResp(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACachePort, "%s: received %s\n", __func__, pkt->print());
    maa->recvCacheTimingResp(pkt);
    outstandingCacheSidePackets--;
    if (blockReason == BlockReason::MAX_XBAR_PACKETS) {
        setUnblocked(BlockReason::MAX_XBAR_PACKETS);
    }
    return true;
}

void MAA::recvCacheTimingSnoopReq(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACachePort, "%s: received %s\n", __func__, pkt->print());
    assert(false);
}
// Express snooping requests to memside port
void MAA::CacheSidePort::recvTimingSnoopReq(PacketPtr pkt) {
    DPRINTF(MAACachePort, "%s: received %s\n", __func__, pkt->print());
    // handle snooping requests
    maa->recvCacheTimingSnoopReq(pkt);
    assert(false);
}

Tick MAA::recvCacheAtomicSnoop(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACachePort, "%s: received %s\n", __func__, pkt->print());
    assert(false);
    return 0;
}
Tick MAA::CacheSidePort::recvAtomicSnoop(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACachePort, "%s: received %s\n", __func__, pkt->print());
    return maa->recvCacheAtomicSnoop(pkt);
    assert(false);
}

void MAA::cacheFunctionalAccess(PacketPtr pkt, bool from_cpu_side) {
    /// print the packet
    DPRINTF(MAACachePort, "%s: received %s\n", __func__, pkt->print());
    assert(false);
}
void MAA::CacheSidePort::recvFunctionalSnoop(PacketPtr pkt) {
    /// print the packet
    // DPRINTF(MAACachePort, "%s: received %s, doing nothing\n", __func__, pkt->print());
    // // functional snoop (note that in contrast to atomic we don't have
    // // a specific functionalSnoop method, as they have the same
    // // behaviour regardless)
    // maa->cacheFunctionalAccess(pkt, false);
    // assert(false);
}

void MAA::CacheSidePort::recvReqRetry() {
    /// print the packet
    DPRINTF(MAACachePort, "%s: called!\n", __func__);
    setUnblocked(BlockReason::CACHE_FAILED);
}

bool MAA::CacheSidePort::sendPacket(uint8_t func_unit_type,
                                    int func_unit_id,
                                    PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACachePort, "%s: UNIT[%s][%d] %s\n",
            __func__,
            func_unit_names[func_unit_type],
            func_unit_id,
            pkt->print());
    if (blockReason != BlockReason::NOT_BLOCKED) {
        DPRINTF(MAACachePort, "%s Send blocked because of %s...\n", __func__,
                blockReason == BlockReason::MAX_XBAR_PACKETS ? "MAX_XBAR_PACKETS" : "CACHE_FAILED");
        funcBlockReasons[func_unit_type][func_unit_id] = blockReason;
        return false;
    }
    if (outstandingCacheSidePackets == maxOutstandingCacheSidePackets) {
        // XBAR is full
        DPRINTF(MAACachePort, "%s Send failed because XBAR is full...\n", __func__);
        assert(blockReason == BlockReason::NOT_BLOCKED);
        blockReason = BlockReason::MAX_XBAR_PACKETS;
        funcBlockReasons[func_unit_type][func_unit_id] = BlockReason::MAX_XBAR_PACKETS;
        return false;
    }
    if (sendTimingReq(pkt) == false) {
        // Cache cannot receive a new request
        DPRINTF(MAACachePort, "%s Send failed because cache returned false...\n", __func__);
        blockReason = BlockReason::CACHE_FAILED;
        funcBlockReasons[func_unit_type][func_unit_id] = BlockReason::CACHE_FAILED;
        return false;
    }
    DPRINTF(MAACachePort, "%s Send is successfull...\n", __func__);
    if (pkt->needsResponse() && !pkt->cacheResponding())
        outstandingCacheSidePackets++;
    return true;
}

void MAA::CacheSidePort::setUnblocked(BlockReason reason) {
    assert(blockReason == reason);
    blockReason = BlockReason::NOT_BLOCKED;
    if (funcBlockReasons[(int)FuncUnitType::INVALIDATOR][0] != BlockReason::NOT_BLOCKED) {
        assert(funcBlockReasons[(int)FuncUnitType::INVALIDATOR][0] == reason);
        assert(maa->invalidator->getState() == Invalidator::Status::Request);
        funcBlockReasons[(int)FuncUnitType::INVALIDATOR][0] = BlockReason::NOT_BLOCKED;
        DPRINTF(MAACachePort, "%s unblocked Unit[invalidator]...\n", __func__);
        maa->invalidator->scheduleExecuteInstructionEvent();
    }
    for (int i = 0; i < maa->num_stream_access_units; i++) {
        if (funcBlockReasons[(int)FuncUnitType::STREAM][i] != BlockReason::NOT_BLOCKED) {
            assert(funcBlockReasons[(int)FuncUnitType::STREAM][i] == reason);
            assert(maa->streamAccessUnits[i].getState() == StreamAccessUnit::Status::Request);
            funcBlockReasons[(int)FuncUnitType::STREAM][i] = BlockReason::NOT_BLOCKED;
            DPRINTF(MAACachePort, "%s unblocked Unit[stream][%d]...\n", __func__, i);
            maa->streamAccessUnits[i].scheduleSendPacketEvent();
        }
    }
    for (int i = 0; i < maa->num_indirect_access_units; i++) {
        if (funcBlockReasons[(int)FuncUnitType::INDIRECT][i] != BlockReason::NOT_BLOCKED) {
            assert(funcBlockReasons[(int)FuncUnitType::INDIRECT][i] == reason);
            assert(maa->indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Request);
            funcBlockReasons[(int)FuncUnitType::INDIRECT][i] = BlockReason::NOT_BLOCKED;
            DPRINTF(MAACachePort, "%s unblocked Unit[indirect][%d]...\n", __func__, i);
            maa->indirectAccessUnits[i].scheduleSendReadPacketEvent();
            maa->indirectAccessUnits[i].scheduleSendWritePacketEvent();
        }
    }
}

void MAA::CacheSidePort::allocate(int _maxOutstandingCacheSidePackets) {
    maxOutstandingCacheSidePackets = _maxOutstandingCacheSidePackets;
    // 16384 is maximum transmitList of PacketQueue (CPU side port of LLC)
    // Taken from gem5-hpc/src/mem/packet_queue.cc (changed from 1024 to 16384)
    maxOutstandingCacheSidePackets = std::min(maxOutstandingCacheSidePackets, 16384);
    // We let it to be 32 less than the maximum
    maxOutstandingCacheSidePackets -= 32;
    funcBlockReasons[(int)FuncUnitType::STREAM] = new BlockReason[maa->num_stream_access_units];
    for (int i = 0; i < maa->num_stream_access_units; i++) {
        funcBlockReasons[(int)FuncUnitType::STREAM][i] = BlockReason::NOT_BLOCKED;
    }
    funcBlockReasons[(int)FuncUnitType::INDIRECT] = new BlockReason[maa->num_indirect_access_units];
    for (int i = 0; i < maa->num_indirect_access_units; i++) {
        funcBlockReasons[(int)FuncUnitType::INDIRECT][i] = BlockReason::NOT_BLOCKED;
    }
    funcBlockReasons[(int)FuncUnitType::INVALIDATOR] = new BlockReason[1];
    funcBlockReasons[(int)FuncUnitType::INVALIDATOR][0] = BlockReason::NOT_BLOCKED;
    blockReason = BlockReason::NOT_BLOCKED;
}

MAA::CacheSidePort::CacheSidePort(const std::string &_name,
                                  MAA *_maa,
                                  const std::string &_label)
    : MAACacheRequestPort(_name, _reqQueue, _snoopRespQueue),
      _reqQueue(*_maa, *this, _snoopRespQueue, _label),
      _snoopRespQueue(*_maa, *this, true, _label), maa(_maa) {
    outstandingCacheSidePackets = 0;
    blockReason = BlockReason::NOT_BLOCKED;
}
} // namespace gem5