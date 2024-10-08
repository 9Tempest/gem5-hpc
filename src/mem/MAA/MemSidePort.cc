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

void MAA::recvMemTimingResp(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAAMemPort, "%s: received %s, cmd: %s, size: %d\n",
            __func__,
            pkt->print(),
            pkt->cmdString(),
            pkt->getSize());
    for (int i = 0; i < pkt->getSize(); i++) {
        DPRINTF(MAAMemPort, "[%d] %02x %s\n", i, pkt->getPtr<uint8_t>()[i], pkt->req->getByteEnable()[i] ? "True" : "False");
    }
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
        for (int i = 0; i < num_indirect_access_units; i++) {
            if (indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Request ||
                indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Drain ||
                indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Response) {
                if (indirectAccessUnits[i].recvData(pkt->getAddr(), pkt->getPtr<uint8_t>(), false)) {
                    panic_if(received, "Received multiple responses for the same request\n");
                }
            }
        }
        for (int i = 0; i < num_stream_access_units; i++) {
            if (streamAccessUnits[i].getState() == StreamAccessUnit::Status::Request ||
                streamAccessUnits[i].getState() == StreamAccessUnit::Status::Response) {
                panic_if(streamAccessUnits[i].recvData(pkt->getAddr(), pkt->getPtr<uint8_t>()),
                         "Received multiple responses for the same request\n");
            }
        }
        break;
    }
    default:
        assert(false);
    }
}
bool MAA::MemSidePort::recvTimingResp(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAAMemPort, "%s: received %s\n", __func__, pkt->print());
    maa->recvMemTimingResp(pkt);
    return true;
}

void MAA::recvMemTimingSnoopReq(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAAMemPort, "%s: received %s\n", __func__, pkt->print());
    assert(false);
}
// Express snooping requests to memside port
void MAA::MemSidePort::recvTimingSnoopReq(PacketPtr pkt) {
    DPRINTF(MAAMemPort, "%s: received %s\n", __func__, pkt->print());
    // handle snooping requests
    maa->recvMemTimingSnoopReq(pkt);
    assert(false);
}

Tick MAA::recvMemAtomicSnoop(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAAMemPort, "%s: received %s\n", __func__, pkt->print());
    assert(false);
    return 0;
}
Tick MAA::MemSidePort::recvAtomicSnoop(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAAMemPort, "%s: received %s\n", __func__, pkt->print());
    return maa->recvMemAtomicSnoop(pkt);
    assert(false);
}

void MAA::memFunctionalAccess(PacketPtr pkt, bool from_cpu_side) {
    /// print the packet
    DPRINTF(MAAMemPort, "%s: received %s\n", __func__, pkt->print());
    assert(false);
}
void MAA::MemSidePort::recvFunctionalSnoop(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAAMemPort, "%s: received %s\n", __func__, pkt->print());
    // functional snoop (note that in contrast to atomic we don't have
    // a specific functionalSnoop method, as they have the same
    // behaviour regardless)
    maa->memFunctionalAccess(pkt, false);
    assert(false);
}

void MAA::MemSidePort::recvReqRetry() {
    /// print the packet
    DPRINTF(MAAMemPort, "%s: called!\n", __func__);
    setUnblocked(BlockReason::MEM_FAILED);
}

void MAA::MemSidePort::setUnblocked(BlockReason reason) {
    assert(blockReason == reason);
    blockReason = BlockReason::NOT_BLOCKED;
    for (int i = 0; i < maa->num_indirect_access_units; i++) {
        if (funcBlockReasons[i] != BlockReason::NOT_BLOCKED) {
            assert(funcBlockReasons[i] == reason);
            assert(maa->indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Request ||
                   maa->indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Drain ||
                   maa->indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Response);
            funcBlockReasons[i] = BlockReason::NOT_BLOCKED;
            DPRINTF(MAAMemPort, "%s unblocked Unit[indirect][%d]...\n", __func__, i);
            maa->indirectAccessUnits[i].scheduleSendReadPacketEvent();
            maa->indirectAccessUnits[i].scheduleSendWritePacketEvent();
        }
    }
}
bool MAA::MemSidePort::sendPacket(int func_unit_id,
                                  PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAAMemPort, "%s: UNIT[INDIRECT][%d] %s\n",
            __func__,
            func_unit_id,
            pkt->print());
    if (blockReason != BlockReason::NOT_BLOCKED) {
        DPRINTF(MAAMemPort, "%s Send blocked because of MEM_FAILED...\n", __func__);
        funcBlockReasons[func_unit_id] = blockReason;
        return false;
    }
    if (maa->memSidePort.sendTimingReq(pkt) == false) {
        // Cache cannot receive a new request
        DPRINTF(MAAMemPort, "%s Send failed because cache returned false...\n", __func__);
        blockReason = BlockReason::MEM_FAILED;
        funcBlockReasons[func_unit_id] = BlockReason::MEM_FAILED;
        return false;
    }
    DPRINTF(MAAMemPort, "%s Send is successfull...\n", __func__);
    return true;
}
void MAA::MemSidePort::allocate() {
    funcBlockReasons = new BlockReason[maa->num_indirect_access_units];
    for (int i = 0; i < maa->num_indirect_access_units; i++) {
        funcBlockReasons[i] = BlockReason::NOT_BLOCKED;
    }
    blockReason = BlockReason::NOT_BLOCKED;
}

void MAA::MAAReqPacketQueue::sendDeferredPacket() {
    /// print the packet
    DPRINTF(MAAMemPort, "%s: called!\n", __func__);
    assert(false);
}

MAA::MemSidePort::MemSidePort(const std::string &_name,
                              MAA *_maa,
                              const std::string &_label)
    : MAAMemRequestPort(_name, _reqQueue, _snoopRespQueue),
      _reqQueue(*_maa, *this, _snoopRespQueue, _label),
      _snoopRespQueue(*_maa, *this, true, _label), maa(_maa) {
    blockReason = BlockReason::NOT_BLOCKED;
}
} // namespace gem5