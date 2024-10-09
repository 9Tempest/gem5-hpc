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
#include "debug/MAACpuPort.hh"
#include "debug/MAAController.hh"
#include <cassert>
#include <cstdint>

#ifndef TRACING_ON
#define TRACING_ON 1
#endif

namespace gem5 {

void MAA::recvTimingSnoopResp(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACpuPort, "%s: received %s\n", __func__, pkt->print());
    switch (pkt->cmd.toInt()) {
    case MemCmd::ReadExResp:
    case MemCmd::ReadResp: {
        assert(pkt->getSize() == 64);
        for (int i = 0; i < 64; i += 4) {
            panic_if(pkt->req->getByteEnable()[i] == false, "Byte enable [%d] is not set for the read response\n", i);
        }
        bool received = false;

        AddressRangeType address_range = AddressRangeType(pkt->getAddr(), addrRanges);
        if (address_range.isValid()) {
            // It's a dirty data for the invalidator in the SPD range
            assert(address_range.getType() == AddressRangeType::Type::SPD_DATA_CACHEABLE_RANGE);
            Addr offset = address_range.getOffset();
            int tile_id = offset / (num_tile_elements * sizeof(uint32_t));
            int element_id = offset % (num_tile_elements * sizeof(uint32_t));
            assert(element_id % sizeof(uint32_t) == 0);
            element_id /= sizeof(uint32_t);
            invalidator->recvData(tile_id, element_id, pkt->getPtr<uint8_t>());
        } else {
            // It's a data
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
        }
        break;
    }
    default:
        assert(false);
    }
}
bool MAA::CpuSidePort::recvTimingSnoopResp(PacketPtr pkt) {
    assert(pkt->isResponse());
    /// print the packet
    DPRINTF(MAACpuPort, "%s: received %s, hasData %s, hasResponseData %s, size %u, isCached %s, satisfied: %d, be:\n",
            __func__,
            pkt->print(),
            pkt->hasData() ? "True" : "False",
            pkt->hasRespData() ? "True" : "False",
            pkt->getSize(),
            pkt->isBlockCached() ? "True" : "False",
            pkt->satisfied());
    for (int i = 0; i < pkt->getSize(); i++) {
        DPRINTF(MAACpuPort, "%02x %s\n", pkt->getPtr<uint8_t>()[i], pkt->req->getByteEnable()[i] ? "True" : "False");
    }
    // assert(false);
    // Express snoop responses from requestor to responder, e.g., from L1 to L2
    maa.recvTimingSnoopResp(pkt);
    outstandingCpuSidePackets--;
    if (blockReason == BlockReason::MAX_XBAR_PACKETS) {
        setUnblocked();
    }
    return true;
}

bool MAA::CpuSidePort::tryTiming(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACpuPort, "%s: received %s\n", __func__, pkt->print());
    return true;
}

void MAA::recvTimingReq(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACpuPort, "%s: received %s, cmd: %s, isMaskedWrite: %d, size: %d\n",
            __func__,
            pkt->print(),
            pkt->cmdString(),
            pkt->isMaskedWrite(),
            pkt->getSize());
    AddressRangeType address_range = AddressRangeType(pkt->getAddr(), addrRanges);
    DPRINTF(MAACpuPort, "%s: address range type: %s\n", __func__, address_range.print());
    for (int i = 0; i < pkt->getSize(); i++) {
        DPRINTF(MAACpuPort, "%02x %s\n", pkt->getPtr<uint8_t>()[i], pkt->req->getByteEnable()[i] ? "True" : "False");
    }
    switch (pkt->cmd.toInt()) {
    case MemCmd::WriteReq: {
        bool respond_immediately = true;
        assert(pkt->isMaskedWrite() == false);
        switch (address_range.getType()) {
        // case AddressRangeType::Type::SPD_DATA_NONCACHEABLE_RANGE: {
        //     Addr offset = address_range.getOffset();
        //     int tile_id = offset / (num_tile_elements * sizeof(uint32_t));
        //     panic_if(pkt->getSize() != 4 && pkt->getSize() != 8, "Invalid size for SPD data: %d\n", pkt->getSize());
        //     int element_id = offset % (num_tile_elements * sizeof(uint32_t));
        //     if (pkt->getSize() == 4) {
        //         assert(element_id % sizeof(uint32_t) == 0);
        //         element_id /= sizeof(uint32_t);
        //         uint32_t data_UINT32 = pkt->getPtr<uint32_t>()[0];
        //         int32_t data_INT32 = pkt->getPtr<int32_t>()[0];
        //         float data_FLOAT = pkt->getPtr<float>()[0];
        //         DPRINTF(MAACpuPort, "%s: TILE[%d][%d] = %u/%d/%f\n", __func__, tile_id, element_id, data_UINT32, data_INT32, data_FLOAT);
        //         spd->setData<uint32_t>(tile_id, element_id, data_UINT32);
        //     } else {
        //         assert(element_id % sizeof(uint64_t) == 0);
        //         element_id /= sizeof(uint64_t);
        //         uint64_t data_UINT64 = pkt->getPtr<uint64_t>()[0];
        //         int64_t data_INT64 = pkt->getPtr<int64_t>()[0];
        //         double data_DOUBLE = pkt->getPtr<double>()[0];
        //         DPRINTF(MAACpuPort, "%s: TILE[%d][%d] = %lu/%ld/%lf\n", __func__, tile_id, element_id, data_UINT64, data_INT64, data_DOUBLE);
        //         spd->setData<uint64_t>(tile_id, element_id, data_UINT64);
        //     }
        //     assert(pkt->needsResponse());
        //     pkt->makeTimingResponse();
        //     cpuSidePort.schedTimingResp(pkt, getClockEdge(spd->setDataLatency(1)));
        //     break;
        // }
        case AddressRangeType::Type::SCALAR_RANGE: {
            Addr offset = address_range.getOffset();
            int element_id = offset % (num_regs * sizeof(uint32_t));
            assert(element_id % sizeof(uint32_t) == 0);
            element_id /= sizeof(uint32_t);
            panic_if(pkt->getSize() != 4 && pkt->getSize() != 8, "Invalid size for SPD data: %d\n", pkt->getSize());
            if (pkt->getSize() == 4) {
                uint32_t data_UINT32 = pkt->getPtr<uint32_t>()[0];
                int32_t data_INT32 = pkt->getPtr<int32_t>()[0];
                float data_FLOAT = pkt->getPtr<float>()[0];
                DPRINTF(MAACpuPort, "%s: REG[%d] = %u/%d/%f\n", __func__, element_id, data_UINT32, data_INT32, data_FLOAT);
                rf->setData<uint32_t>(element_id, data_UINT32);
            } else {
                uint64_t data_UINT64 = pkt->getPtr<uint64_t>()[0];
                int64_t data_INT64 = pkt->getPtr<int64_t>()[0];
                double data_DOUBLE = pkt->getPtr<double>()[0];
                DPRINTF(MAACpuPort, "%s: REG[%d] = %lu/%ld/%lf\n", __func__, element_id, data_UINT64, data_INT64, data_DOUBLE);
                rf->setData<uint64_t>(element_id, data_UINT64);
            }
            assert(pkt->needsResponse());
            pkt->makeTimingResponse();
            cpuSidePort.schedTimingResp(pkt, getClockEdge(Cycles(1)));
            break;
        }
        case AddressRangeType::Type::INSTRUCTION_RANGE: {
            Addr offset = address_range.getOffset();
            int element_id = offset % (num_instructions * sizeof(uint64_t));
            assert(element_id % sizeof(uint64_t) == 0);
            element_id /= sizeof(uint64_t);
            uint64_t data = pkt->getPtr<uint64_t>()[0];
            DPRINTF(MAACpuPort, "%s: IF[%d] = %ld\n", __func__, element_id, data);
#define NA_UINT8 0xFF
            switch (element_id) {
            case 0: {
                current_instruction->dst2SpdID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction->dst1SpdID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction->optype = (data & NA_UINT8) == NA_UINT8 ? Instruction::OPType::MAX : static_cast<Instruction::OPType>(data & NA_UINT8);
                data = data >> 8;
                current_instruction->datatype = (data & NA_UINT8) == NA_UINT8 ? Instruction::DataType::MAX : static_cast<Instruction::DataType>(data & NA_UINT8);
                assert(current_instruction->datatype != Instruction::DataType::MAX);
                data = data >> 8;
                current_instruction->opcode = (data & NA_UINT8) == NA_UINT8 ? Instruction::OpcodeType::MAX : static_cast<Instruction::OpcodeType>(data & NA_UINT8);
                assert(current_instruction->opcode != Instruction::OpcodeType::MAX);
                break;
            }
            case 1: {
                current_instruction->condSpdID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction->src3RegID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction->src2RegID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction->src1RegID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction->dst2RegID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction->dst1RegID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction->src2SpdID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction->src1SpdID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                break;
            }
            case 2: {
                current_instruction->baseAddr = data;
                current_instruction->state = Instruction::Status::Idle;
                current_instruction->CID = pkt->req->contextId();
                current_instruction->PC = pkt->req->getPC();
                DPRINTF(MAAController, "%s: %s received!\n", __func__, current_instruction->print());
                respond_immediately = false;
                panic_if(my_outstanding_instruction_pkt, "Received multiple instruction packets\n");
                my_outstanding_instruction_pkt = true;
                my_instruction_pkt = pkt;
                scheduleDispatchInstructionEvent();
                break;
            }
            default:
                assert(false);
            }
            assert(pkt->needsResponse());
            if (respond_immediately) {
                pkt->makeTimingResponse();
                cpuSidePort.schedTimingResp(pkt, getClockEdge(Cycles(1)));
            }
            break;
        }
        default:
            // Write to SPD_DATA_CACHEABLE_RANGE not possible. All SPD writes must be to SPD_DATA_NONCACHEABLE_RANGE
            // Write to SPD_SIZE_RANGE not possible. Size is read-only.
            // Write to SPD_READY_RANGE not possible. Ready is read-only.
            DPRINTF(MAAController, "%s: Error: Range(%s) and cmd(%s) is illegal\n",
                    __func__, address_range.print(), pkt->cmdString());
            assert(false);
        }
        break;
    }
    case MemCmd::ReadReq: {
        // all read responses have a data payload
        assert(pkt->hasRespData());
        switch (address_range.getType()) {
        case AddressRangeType::Type::SPD_SIZE_RANGE: {
            assert(pkt->getSize() == sizeof(uint16_t));
            Addr offset = address_range.getOffset();
            assert(offset % sizeof(uint16_t) == 0);
            int element_id = offset / sizeof(uint16_t);
            uint16_t data = spd->getSize(element_id);
            uint8_t *dataPtr = (uint8_t *)(&data);
            pkt->setData(dataPtr);
            assert(pkt->needsResponse());
            pkt->makeTimingResponse();
            cpuSidePort.schedTimingResp(pkt, getClockEdge(Cycles(1)));
            break;
        }
        case AddressRangeType::Type::SPD_READY_RANGE: {
            assert(pkt->getSize() == sizeof(uint16_t));
            Addr offset = address_range.getOffset();
            assert(offset % sizeof(uint16_t) == 0);
            my_ready_tile_id = offset / sizeof(uint16_t);
            const uint16_t one = 1;
            pkt->setData((const uint8_t *)&one);
            assert(pkt->needsResponse());
            if (spd->getTileReady(my_ready_tile_id)) {
                pkt->makeTimingResponse();
                cpuSidePort.schedTimingResp(pkt, getClockEdge(Cycles(1)));
            } else {
                panic_if(my_outstanding_ready_pkt, "Received multiple ready read packets\n");
                my_outstanding_ready_pkt = true;
                my_ready_pkt = pkt;
            }
            break;
        }
        case AddressRangeType::Type::SCALAR_RANGE: {
            panic_if(pkt->getSize() != 4 && pkt->getSize() != 8, "Invalid size for SPD data: %d\n", pkt->getSize());
            Addr offset = address_range.getOffset();
            int element_id = offset % (num_regs * sizeof(uint32_t));
            assert(element_id % sizeof(uint32_t) == 0);
            element_id /= sizeof(uint32_t);
            uint8_t *dataPtr = rf->getDataPtr(element_id);
            pkt->setData(dataPtr);
            assert(pkt->needsResponse());
            pkt->makeTimingResponse();
            cpuSidePort.schedTimingResp(pkt, getClockEdge(Cycles(1)));
            break;
        }
        default: {
            // Read from SPD_DATA_CACHEABLE_RANGE uses ReadSharedReq command.
            // Read from SPD_DATA_NONCACHEABLE_RANGE not possible. All SPD reads must be from SPD_DATA_CACHEABLE_RANGE.
            DPRINTF(MAAController, "%s: Error: Range(%s) and cmd(%s) is illegal\n",
                    __func__, address_range.print(), pkt->cmdString());
            assert(false);
        }
        }
        break;
    }
    case MemCmd::ReadExReq:
    case MemCmd::ReadSharedReq: {
        // all read responses have a data payload
        assert(pkt->hasRespData());
        switch (address_range.getType()) {
        case AddressRangeType::Type::SPD_DATA_CACHEABLE_RANGE: {
            Addr offset = address_range.getOffset();
            int tile_id = offset / (num_tile_elements * sizeof(uint32_t));
            int element_id = offset % (num_tile_elements * sizeof(uint32_t));
            assert(element_id % sizeof(uint32_t) == 0);
            element_id /= sizeof(uint32_t);
            spd->setTileDirty(tile_id, 4);
            if (pkt->cmd == MemCmd::ReadSharedReq) {
                invalidator->read(tile_id, element_id);
            } else {
                invalidator->write(tile_id, element_id);
            }
            uint8_t *dataPtr = spd->getDataPtr(tile_id, element_id);
            pkt->setData(dataPtr);
            assert(pkt->needsResponse());
            pkt->makeTimingResponse();
            cpuSidePort.schedTimingResp(pkt, getClockEdge(spd->getDataLatency(1)));
            break;
        }
        default:
            DPRINTF(MAAController, "%s: Error: Range(%s) and cmd(%s) is illegal\n",
                    __func__, address_range.print(), pkt->cmdString());
            assert(false);
        }
        break;
    }
    default:
        assert(false);
    }
}
bool MAA::CpuSidePort::recvTimingReq(PacketPtr pkt) {
    assert(pkt->isRequest());
    /// print the packet
    DPRINTF(MAACpuPort, "%s: received %s\n", __func__, pkt->print());

    if (tryTiming(pkt)) {
        maa.recvTimingReq(pkt);
        return true;
    }
    return false;
}

void MAA::CpuSidePort::recvFunctional(PacketPtr pkt) {
    assert(false);
}

Tick MAA::recvAtomic(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACpuPort, "%s: received %s\n", __func__, pkt->print());
    assert(false);
    return 0;
}
Tick MAA::CpuSidePort::recvAtomic(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACpuPort, "%s: received %s\n", __func__, pkt->print());
    assert(false);
    return maa.recvAtomic(pkt);
}

AddrRangeList MAA::CpuSidePort::getAddrRanges() const {
    return maa.getAddrRanges();
}

bool MAA::CpuSidePort::sendSnoopPacket(uint8_t func_unit_type,
                                       int func_unit_id,
                                       PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACpuPort, "%s: UNIT[%s][%d] %s\n",
            __func__,
            func_unit_names[func_unit_type],
            func_unit_id,
            pkt->print());
    panic_if(pkt->isExpressSnoop() == false, "Packet is not an express snoop packet\n");
    panic_if(func_unit_type == (int)FuncUnitType::STREAM, "Stream does not have any snoop requests\n");
    if (blockReason != BlockReason::NOT_BLOCKED) {
        DPRINTF(MAACpuPort, "%s Send snoop blocked because of MAX_XBAR_PACKETS...\n", __func__);
        funcBlockReasons[func_unit_type][func_unit_id] = blockReason;
        return false;
    }
    if (outstandingCpuSidePackets == maxOutstandingCpuSidePackets) {
        // XBAR is full
        DPRINTF(MAACpuPort, "%s Send failed because XBAR is full...\n", __func__);
        assert(blockReason == BlockReason::NOT_BLOCKED);
        blockReason = BlockReason::MAX_XBAR_PACKETS;
        funcBlockReasons[func_unit_type][func_unit_id] = BlockReason::MAX_XBAR_PACKETS;
        return false;
    }
    sendTimingSnoopReq(pkt);
    DPRINTF(MAACpuPort, "%s Send is successfull...\n", __func__);
    if (pkt->cacheResponding())
        outstandingCpuSidePackets++;
    return true;
}

void MAA::CpuSidePort::setUnblocked() {
    blockReason = BlockReason::NOT_BLOCKED;
    if (funcBlockReasons[(int)FuncUnitType::INVALIDATOR][0] != BlockReason::NOT_BLOCKED) {
        assert(maa.invalidator->getState() == Invalidator::Status::Request);
        funcBlockReasons[(int)FuncUnitType::INVALIDATOR][0] = BlockReason::NOT_BLOCKED;
        DPRINTF(MAACpuPort, "%s unblocked Unit[invalidator]...\n", __func__);
        maa.invalidator->scheduleExecuteInstructionEvent();
    }
    for (int i = 0; i < maa.num_indirect_access_units; i++) {
        if (funcBlockReasons[(int)FuncUnitType::INDIRECT][i] != BlockReason::NOT_BLOCKED) {
            assert(maa.indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Request ||
                   maa.indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Drain);
            funcBlockReasons[(int)FuncUnitType::INDIRECT][i] = BlockReason::NOT_BLOCKED;
            DPRINTF(MAACpuPort, "%s unblocked Unit[indirect][%d]...\n", __func__, i);
            maa.indirectAccessUnits[i].scheduleSendReadPacketEvent();
        }
    }
}

void MAA::CpuSidePort::allocate(int _maxOutstandingCpuSidePackets) {
    outstandingCpuSidePackets = 0;
    maxOutstandingCpuSidePackets = _maxOutstandingCpuSidePackets - 16;
    funcBlockReasons[(int)FuncUnitType::INDIRECT] = new BlockReason[maa.num_indirect_access_units];
    for (int i = 0; i < maa.num_indirect_access_units; i++) {
        funcBlockReasons[(int)FuncUnitType::INDIRECT][i] = BlockReason::NOT_BLOCKED;
    }
    funcBlockReasons[(int)FuncUnitType::INVALIDATOR] = new BlockReason[1];
    funcBlockReasons[(int)FuncUnitType::INVALIDATOR][0] = BlockReason::NOT_BLOCKED;
    blockReason = BlockReason::NOT_BLOCKED;
}

MAA::CpuSidePort::CpuSidePort(const std::string &_name, MAA &_maa,
                              const std::string &_label)
    : MAAResponsePort(_name, _maa, _label) {
}
} // namespace gem5