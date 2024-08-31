#include "mem/MAA/MAA.hh"
#include "base/addr_range.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "mem/packet.hh"
#include "params/MAA.hh"
#include "debug/MAA.hh"
#include <cassert>
#include <cstdint>

#ifndef TRACING_ON
#define TRACING_ON 1
#endif

namespace gem5 {

MAA::MAAResponsePort::MAAResponsePort(const std::string &_name, MAA &_maa, const std::string &_label)
    : QueuedResponsePort(_name, queue),
      maa{_maa},
      queue(_maa, *this, true, _label) {
}

MAA::MAA(const MAAParams &p)
    : ClockedObject(p),
      cpuSidePort(p.name + ".cpu_side_port", *this, "CpuSidePort"),
      memSidePort(p.name + ".mem_side_port", this, "MemSidePort"),
      cacheSidePort(p.name + ".cache_side_port", this, "CacheSidePort"),
      addrRanges(p.addr_ranges.begin(), p.addr_ranges.end()),
      num_tiles(p.num_tiles),
      num_tile_elements(p.num_tile_elements),
      num_regs(p.num_regs),
      num_instructions(p.num_instructions),
      num_stream_access_units(p.num_stream_access_units),
      num_indirect_access_units(p.num_indirect_access_units),
      num_range_units(p.num_range_units),
      num_alu_units(p.num_alu_units),
      system(p.system),
      mmu(p.mmu) {

    requestorId = p.system->getRequestorId(this);
    spd = new SPD(num_tiles, num_tile_elements);
    rf = new RF(num_regs);
    ifile = new IF(num_instructions);
    streamAccessUnits = new StreamAccessUnit[num_stream_access_units];
    for (int i = 0; i < num_stream_access_units; i++) {
        streamAccessUnits[i].allocate(num_tile_elements, this);
    }
}

void MAA::init() {
    if (!cpuSidePort.isConnected())
        fatal("Cache ports on %s are not connected\n", name());
    cpuSidePort.sendRangeChange();
}

MAA::~MAA() {
}

Port &MAA::getPort(const std::string &if_name, PortID idx) {
    if (if_name == "mem_side") {
        return memSidePort;
    } else if (if_name == "cpu_side") {
        return cpuSidePort;
    } else if (if_name == "cache_side") {
        return cacheSidePort;
    } else {
        return ClockedObject::getPort(if_name, idx);
    }
}

int MAA::inRange(Addr addr) const {
    int r_id = -1;
    for (const auto &r : addrRanges) {
        if (r.contains(addr)) {
            break;
        }
    }
    return r_id;
}

///////////////
//
// CpuSidePort
//
///////////////
void MAA::recvTimingSnoopResp(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    assert(false);
}
bool MAA::CpuSidePort::recvTimingSnoopResp(PacketPtr pkt) {
    assert(pkt->isResponse());
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    assert(false);
    // Express snoop responses from requestor to responder, e.g., from L1 to L2
    maa.recvTimingSnoopResp(pkt);
    return true;
}

bool MAA::CpuSidePort::tryTiming(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    return true;
}

void MAA::recvTimingReq(PacketPtr pkt) {
    /// print the packet
    AddressRangeType address_range = AddressRangeType(pkt->getAddr(), addrRanges);
    DPRINTF(MAA, "%s: received %s, %s, cmd: %s, isMaskedWrite: %d, size: %d\n",
            __func__,
            pkt->print(),
            address_range.print(),
            pkt->cmdString(),
            pkt->isMaskedWrite(),
            pkt->getSize());
    for (int i = 0; i < pkt->getSize(); i++) {
        DPRINTF(MAA, "%02x %s\n", pkt->getPtr<uint8_t>()[i], pkt->req->getByteEnable()[i] ? "True" : "False");
    }
    switch (pkt->cmd.toInt()) {
    case MemCmd::WriteReq: {
        assert(pkt->isMaskedWrite() == false);
        switch (address_range.getType()) {
        case AddressRangeType::Type::SPD_DATA_NONCACHEABLE_RANGE: {
            assert(pkt->getSize() == 4);
            Addr offset = address_range.getOffset();
            int tile_id = offset / (num_tile_elements * sizeof(uint32_t));
            int element_id = offset % (num_tile_elements * sizeof(uint32_t));
            assert(element_id % sizeof(uint32_t) == 0);
            element_id /= sizeof(uint32_t);
            uint32_t data = pkt->getPtr<uint32_t>()[0];
            DPRINTF(MAA, "%s: TILE[%d][%d] = %d\n", __func__, tile_id, element_id, data);
            spd->setData(tile_id, element_id, data);
            assert(pkt->needsResponse());
            pkt->makeTimingResponse();
            Cycles lat = Cycles(2);
            Tick request_time = clockEdge(lat);
            cpuSidePort.schedTimingResp(pkt, request_time);
            break;
        }
        case AddressRangeType::Type::SCALAR_RANGE: {
            Addr offset = address_range.getOffset();
            int element_id = offset % (num_regs * sizeof(uint32_t));
            assert(element_id % sizeof(uint32_t) == 0);
            element_id /= sizeof(uint32_t);
            uint32_t data = pkt->getPtr<uint32_t>()[0];
            DPRINTF(MAA, "%s: REG[%d] = %d\n", __func__, element_id, data);
            rf->setData(element_id, data);
            assert(pkt->needsResponse());
            pkt->makeTimingResponse();
            Cycles lat = Cycles(2);
            Tick request_time = clockEdge(lat);
            cpuSidePort.schedTimingResp(pkt, request_time);
            break;
        }
        case AddressRangeType::Type::INSTRUCTION_RANGE: {
            Addr offset = address_range.getOffset();
            int element_id = offset % (num_instructions * sizeof(uint64_t));
            assert(element_id % sizeof(uint64_t) == 0);
            element_id /= sizeof(uint64_t);
            uint64_t data = pkt->getPtr<uint64_t>()[0];
            DPRINTF(MAA, "%s: IF[%d] = %ld\n", __func__, element_id, data);
#define NA_UINT8 0xFF
            switch (element_id) {
            case 0: {
                current_instruction.dst2SpdID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction.dst1SpdID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction.optype = (data & NA_UINT8) == NA_UINT8 ? Instruction::OPType::MAX : static_cast<Instruction::OPType>(data & NA_UINT8);
                data = data >> 8;
                current_instruction.datatype = (data & NA_UINT8) == NA_UINT8 ? Instruction::DataType::MAX : static_cast<Instruction::DataType>(data & NA_UINT8);
                assert(current_instruction.datatype != Instruction::DataType::MAX);
                data = data >> 8;
                current_instruction.opcode = (data & NA_UINT8) == NA_UINT8 ? Instruction::OpcodeType::MAX : static_cast<Instruction::OpcodeType>(data & NA_UINT8);
                assert(current_instruction.opcode != Instruction::OpcodeType::MAX);
                break;
            }
            case 1: {
                current_instruction.condSpdID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction.src3RegID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction.src2RegID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction.src1RegID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction.dst2RegID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction.dst1RegID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction.src2SpdID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                current_instruction.src1SpdID = (data & NA_UINT8) == NA_UINT8 ? -1 : (data & NA_UINT8);
                data = data >> 8;
                break;
            }
            case 2: {
                current_instruction.baseAddr = data;
                current_instruction.state = Instruction::Status::Idle;
                current_instruction.CID = pkt->req->contextId();
                current_instruction.PC = pkt->req->getPC();
                bool ready = true;
                ready &= current_instruction.src1SpdID == -1 ? true : spd->getReady(current_instruction.src1SpdID);
                ready &= current_instruction.src2SpdID == -1 ? true : spd->getReady(current_instruction.src2SpdID);
                assert(ifile->pushInstruction(current_instruction, ready));
                if (current_instruction.dst1SpdID != -1) {
                    assert(current_instruction.dst1SpdID != current_instruction.src1SpdID);
                    assert(current_instruction.dst1SpdID != current_instruction.src2SpdID);
                    spd->unsetReady(current_instruction.dst1SpdID);
                }
                if (current_instruction.dst2SpdID != -1) {
                    assert(current_instruction.dst2SpdID != current_instruction.src1SpdID);
                    assert(current_instruction.dst2SpdID != current_instruction.src2SpdID);
                    spd->unsetReady(current_instruction.dst2SpdID);
                }
                DPRINTF(MAA, "%s: %s pushed, ready: %d!\n", __func__, current_instruction.print(), ready);
                issueInstruction();
                break;
            }
            default:
                assert(false);
            }
            assert(pkt->needsResponse());
            pkt->makeTimingResponse();
            Cycles lat = Cycles(2);
            Tick request_time = clockEdge(lat);
            cpuSidePort.schedTimingResp(pkt, request_time);
            break;
        }
        default:
            // Write to SPD_DATA_CACHEABLE_RANGE not possible. All SPD writes must be to SPD_DATA_NONCACHEABLE_RANGE
            // Write to SPD_SIZE_RANGE not possible. Size is read-only.
            // Write to SPD_READY_RANGE not possible. Ready is read-only.
            DPRINTF(MAA, "%s: Error: Range(%s) and cmd(%s) is illegal\n",
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
            Cycles lat = Cycles(2);
            Tick request_time = clockEdge(lat);
            cpuSidePort.schedTimingResp(pkt, request_time);
            break;
        }
        case AddressRangeType::Type::SPD_READY_RANGE: {
            assert(pkt->getSize() == sizeof(uint16_t));
            Addr offset = address_range.getOffset();
            assert(offset % sizeof(uint16_t) == 0);
            int element_id = offset / sizeof(uint16_t);
            uint16_t data = spd->getReady(element_id);
            uint8_t *dataPtr = (uint8_t *)(&data);
            pkt->setData(dataPtr);
            assert(pkt->needsResponse());
            pkt->makeTimingResponse();
            Cycles lat = Cycles(2);
            Tick request_time = clockEdge(lat);
            cpuSidePort.schedTimingResp(pkt, request_time);
            break;
        }
        case AddressRangeType::Type::SCALAR_RANGE: {
            assert(pkt->getSize() == sizeof(uint32_t));
            Addr offset = address_range.getOffset();
            int element_id = offset % (num_regs * sizeof(uint32_t));
            assert(element_id % sizeof(uint32_t) == 0);
            element_id /= sizeof(uint32_t);
            uint8_t *dataPtr = rf->getDataPtr(element_id);
            pkt->setData(dataPtr);
            assert(pkt->needsResponse());
            pkt->makeTimingResponse();
            Cycles lat = Cycles(2);
            Tick request_time = clockEdge(lat);
            cpuSidePort.schedTimingResp(pkt, request_time);
            break;
        }
        default: {
            // Read from SPD_DATA_CACHEABLE_RANGE uses ReadSharedReq command.
            // Read from SPD_DATA_NONCACHEABLE_RANGE not possible. All SPD reads must be from SPD_DATA_CACHEABLE_RANGE.
            DPRINTF(MAA, "%s: Error: Range(%s) and cmd(%s) is illegal\n",
                    __func__, address_range.print(), pkt->cmdString());
            assert(false);
        }
        }
        break;
    }
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
            uint8_t *dataPtr = spd->getDataPtr(tile_id, element_id);
            pkt->setData(dataPtr);
            assert(pkt->needsResponse());
            pkt->makeTimingResponse();
            Cycles lat = Cycles(2);
            Tick request_time = clockEdge(lat);
            cpuSidePort.schedTimingResp(pkt, request_time);
            break;
        }
        default:
            DPRINTF(MAA, "%s: Error: Range(%s) and cmd(%s) is illegal\n",
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
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());

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
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    assert(false);
    return 0;
}
Tick MAA::CpuSidePort::recvAtomic(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    assert(false);
    return maa.recvAtomic(pkt);
}

AddrRangeList MAA::CpuSidePort::getAddrRanges() const {
    return maa.getAddrRanges();
}

MAA::CpuSidePort::CpuSidePort(const std::string &_name, MAA &_maa,
                              const std::string &_label)
    : MAAResponsePort(_name, _maa, _label) {
}

///////////////
//
// MemSidePort
//
///////////////
void MAA::recvMemTimingResp(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s, cmd: %s, size: %d\n",
            __func__,
            pkt->print(),
            pkt->cmdString(),
            pkt->getSize());
    for (int i = 0; i < pkt->getSize(); i++) {
        DPRINTF(MAA, "%02x %s\n", pkt->getPtr<uint8_t>()[i], pkt->req->getByteEnable()[i] ? "True" : "False");
    }
    switch (pkt->cmd.toInt()) {
    case MemCmd::ReadResp: {
        // Data must be routed to the indirect access
        assert(false);

        // assert(pkt->getSize() == 64);
        // std::vector<uint32_t> data;
        // std::vector<uint16_t> wid;
        // for (int i = 0; i < 64; i += 4) {
        //     if (pkt->req->getByteEnable()[i] == true) {
        //         data.push_back(*(pkt->getPtr<uint32_t>() + i / 4));
        //         wid.push_back(i / 4);
        //     }
        // }
        // for (int i = 0; i < num_stream_access_units; i++) {
        //     if (streamAccessUnits[i].getState() == StreamAccessUnit::Status::Request ||
        //         streamAccessUnits[i].getState() == StreamAccessUnit::Status::Response) {
        //         streamAccessUnits[i].recvData(pkt->getAddr(), data, wid);
        //     }
        // }
        break;
    }
    default:
        assert(false);
    }
}
bool MAA::MemSidePort::recvTimingResp(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    maa->recvMemTimingResp(pkt);
    return true;
}

void MAA::recvMemTimingSnoopReq(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    assert(false);
}
// Express snooping requests to memside port
void MAA::MemSidePort::recvTimingSnoopReq(PacketPtr pkt) {
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    // handle snooping requests
    maa->recvMemTimingSnoopReq(pkt);
    assert(false);
}

Tick MAA::recvMemAtomicSnoop(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    assert(false);
    return 0;
}
Tick MAA::MemSidePort::recvAtomicSnoop(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    return maa->recvMemAtomicSnoop(pkt);
    assert(false);
}

void MAA::memFunctionalAccess(PacketPtr pkt, bool from_cpu_side) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    assert(false);
}
void MAA::MemSidePort::recvFunctionalSnoop(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    // functional snoop (note that in contrast to atomic we don't have
    // a specific functionalSnoop method, as they have the same
    // behaviour regardless)
    maa->memFunctionalAccess(pkt, false);
    assert(false);
}

void MAA::MemSidePort::recvReqRetry() {
    /// print the packet
    DPRINTF(MAA, "%s: called!\n", __func__);
    // this will be used for the indirect access
    assert(false);
    for (int i = 0; i < maa->num_stream_access_units; i++) {
        if (maa->streamAccessUnits[i].getState() == StreamAccessUnit::Status::Request) {
            maa->streamAccessUnits[i].execute();
        }
    }
}

void MAA::MAAReqPacketQueue::sendDeferredPacket() {
    /// print the packet
    DPRINTF(MAA, "%s: called!\n", __func__);
    assert(false);
}

MAA::MemSidePort::MemSidePort(const std::string &_name,
                              MAA *_maa,
                              const std::string &_label)
    : MAAMemRequestPort(_name, _reqQueue, _snoopRespQueue),
      _reqQueue(*_maa, *this, _snoopRespQueue, _label),
      _snoopRespQueue(*_maa, *this, true, _label), maa(_maa) {
}

///////////////
//
// CacheSidePort
//
///////////////
void MAA::recvCacheTimingResp(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s, cmd: %s, size: %d\n",
            __func__,
            pkt->print(),
            pkt->cmdString(),
            pkt->getSize());
    for (int i = 0; i < pkt->getSize(); i++) {
        DPRINTF(MAA, "%02x %s\n", pkt->getPtr<uint8_t>()[i], pkt->req->getByteEnable()[i] ? "True" : "False");
    }
    switch (pkt->cmd.toInt()) {
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
        for (int i = 0; i < num_stream_access_units; i++) {
            if (streamAccessUnits[i].getState() == StreamAccessUnit::Status::Request ||
                streamAccessUnits[i].getState() == StreamAccessUnit::Status::Response) {
                streamAccessUnits[i].recvData(pkt->getAddr(), data, wid);
            }
        }
        break;
    }
    default:
        assert(false);
    }
}
bool MAA::CacheSidePort::recvTimingResp(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    maa->recvCacheTimingResp(pkt);
    return true;
}

void MAA::recvCacheTimingSnoopReq(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    assert(false);
}
// Express snooping requests to memside port
void MAA::CacheSidePort::recvTimingSnoopReq(PacketPtr pkt) {
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    // handle snooping requests
    maa->recvCacheTimingSnoopReq(pkt);
    assert(false);
}

Tick MAA::recvCacheAtomicSnoop(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    assert(false);
    return 0;
}
Tick MAA::CacheSidePort::recvAtomicSnoop(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    return maa->recvCacheAtomicSnoop(pkt);
    assert(false);
}

void MAA::cacheFunctionalAccess(PacketPtr pkt, bool from_cpu_side) {
    /// print the packet
    DPRINTF(MAA, "%s: received %s\n", __func__, pkt->print());
    assert(false);
}
void MAA::CacheSidePort::recvFunctionalSnoop(PacketPtr pkt) {
    /// print the packet
    // DPRINTF(MAA, "%s: received %s, doing nothing\n", __func__, pkt->print());
    // // functional snoop (note that in contrast to atomic we don't have
    // // a specific functionalSnoop method, as they have the same
    // // behaviour regardless)
    // maa->cacheFunctionalAccess(pkt, false);
    // assert(false);
}

void MAA::CacheSidePort::recvReqRetry() {
    /// print the packet
    DPRINTF(MAA, "%s: called!\n", __func__);
    for (int i = 0; i < maa->num_stream_access_units; i++) {
        if (maa->streamAccessUnits[i].getState() == StreamAccessUnit::Status::Request) {
            maa->streamAccessUnits[i].execute();
        }
    }
}

MAA::CacheSidePort::CacheSidePort(const std::string &_name,
                                  MAA *_maa,
                                  const std::string &_label)
    : MAACacheRequestPort(_name, _reqQueue, _snoopRespQueue),
      _reqQueue(*_maa, *this, _snoopRespQueue, _label),
      _snoopRespQueue(*_maa, *this, true, _label), maa(_maa) {
}

///////////////
//
// MAA
//
///////////////

void MAA::issueInstruction() {
    for (int i = 0; i < num_stream_access_units; i++) {
        if (streamAccessUnits[i].getState() == StreamAccessUnit::Status::Idle) {
            Instruction *inst = ifile->getReady(Instruction::FuncUnitType::STREAM);
            if (inst != nullptr) {
                streamAccessUnits[i].execute(inst);
            }
        }
    }
}

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

void StreamAccessUnit::execute(Instruction *_instruction) {
    switch (state) {
    case Status::Idle: {
        assert(_instruction != nullptr);
        DPRINTF(MAA, "%s: idling %s!\n", __func__, _instruction->print());
        state = Status::Decode;
        [[fallthrough]];
    }
    case Status::Decode: {
        assert(_instruction != nullptr);
        DPRINTF(MAA, "%s: decoding %s!\n", __func__, _instruction->print());
        my_instruction = _instruction;

        // Decoding the instruction
        my_dst_tile = my_instruction->dst1SpdID;
        my_cond_tile = my_instruction->condSpdID;
        my_min = *((int *)maa->rf->getDataPtr(my_instruction->src1RegID));
        my_max = *((int *)maa->rf->getDataPtr(my_instruction->src2RegID));
        my_stride = *((int *)maa->rf->getDataPtr(my_instruction->src3RegID));

        // Initialization
        my_i = my_min;
        my_last_block_addr = 0;
        my_idx = 0;
        my_base_addr = my_instruction->baseAddr;
        my_byte_enable = std::vector<bool>(block_size, false);
        my_outstanding_pkt = false;
        my_received_responses = 0;

        // Setting the state of the instruction and stream unit
        my_instruction->state = Instruction::Status::Service;
        state = Status::Request;
        [[fallthrough]];
    }
    case Status::Request: {
        DPRINTF(MAA, "%s: requesting %s!\n", __func__, my_instruction->print());
        if (my_outstanding_pkt) {
            if (sendOutstandingPacket() == false) {
                break;
            }
        }
        for (; my_i < my_max && my_idx < maa->num_tile_elements; my_i += my_stride, my_idx++) {
            if (my_cond_tile == -1 || maa->spd->getData(my_cond_tile, my_idx) != 0) {
                Addr curr_addr = my_base_addr + word_size * my_i;
                Addr curr_block_addr = addrBlockAlign(curr_addr, block_size);
                if (curr_block_addr != my_last_block_addr) {
                    if (my_last_block_addr != 0) {
                        createMyPacket();
                        if (sendOutstandingPacket() == false) {
                            return;
                        }
                    }
                    my_last_block_addr = curr_block_addr;
                    translatePacket();
                }
                uint16_t base_byte_id = curr_addr - curr_block_addr;
                uint16_t word_id = base_byte_id / word_size;
                for (int byte_id = base_byte_id; byte_id < base_byte_id + word_size; byte_id++) {
                    assert((byte_id >= 0) && (byte_id < block_size));
                    my_byte_enable[byte_id] = true;
                }
                request_table->add_entry(my_idx, my_translated_physical_address, word_id);
                DPRINTF(MAA, "RequestTable: entry %d added! vaddr=0x%lx, paddr=0x%lx wid = %d\n",
                        my_idx, curr_block_addr, my_translated_physical_address, word_id);
            }
        }
        if (my_last_block_addr != 0) {
            assert(std::find(my_byte_enable.begin(), my_byte_enable.end(), true) != my_byte_enable.end());
            createMyPacket();
            if (sendOutstandingPacket() == false) {
                break;
            }
        }
        state = Status::Response;
        maa->spd->setSize(my_dst_tile, my_idx);
        break;
    }
    case Status::Response: {
        DPRINTF(MAA, "%s: responding %s!\n", __func__, my_instruction->print());
        if (my_received_responses == my_idx) {
            my_instruction->state = Instruction::Status::Finish;
            state = Status::Idle;
            maa->spd->setReady(my_dst_tile);
        }
        break;
    }
    default:
        assert(false);
    }
}
void StreamAccessUnit::createMyPacket() {
    /**** Packet generation ****/
    RequestPtr real_req = std::make_shared<Request>(my_translated_physical_address, block_size, flags, maa->requestorId);
    real_req->setByteEnable(my_byte_enable);
    my_pkt = new Packet(real_req, MemCmd::ReadSharedReq);
    my_pkt->allocate();
    my_outstanding_pkt = true;
    DPRINTF(MAA, "%s: created %s, be:\n", __func__, my_pkt->print());
    for (int j = 0; j < block_size; j++) {
        DPRINTF(MAA, "[%d] %s\n", j, my_byte_enable[j] ? "T" : "F");
    }
    DPRINTF(MAA, "\n");
    my_last_block_addr = 0;
    std::fill(my_byte_enable.begin(), my_byte_enable.end(), false);
}
bool StreamAccessUnit::sendOutstandingPacket() {
    DPRINTF(MAA, "%s: trying sending %s, be:\n", __func__, my_pkt->print());
    if (maa->cacheSidePort.sendTimingReq(my_pkt) == false) {
        DPRINTF(MAA, "%s: send failed, leaving execution...\n", __func__);
        return false;
    } else {
        my_outstanding_pkt = false;
        return true;
    }
}
void StreamAccessUnit::translatePacket() {
    /**** Address translation ****/
    RequestPtr translation_req = std::make_shared<Request>(my_last_block_addr, block_size, flags, maa->requestorId, my_instruction->PC, my_instruction->CID);
    ThreadContext *tc = maa->system->threads[my_instruction->CID];
    maa->mmu->translateTiming(translation_req, tc, this, BaseMMU::Read);
    // The above function immediately does the translation and calls the finish function
    assert(translation_done);
    translation_done = false;
}
void StreamAccessUnit::recvData(const Addr addr,
                                std::vector<uint32_t> data,
                                std::vector<uint16_t> wids) {
    std::vector<RequestTableEntry> entries = request_table->get_entries(addr);
    if (entries.empty()) {
        return;
    }

    int num_words = data.size();
    assert(num_words == wids.size());
    assert(num_words == entries.size());

    DPRINTF(MAA, "%s: addr(0x%lx), (RT) wids: \n", __func__, addr);
    for (int i = 0; i < num_words; i++) {
        DPRINTF(MAA, " %d | %d\n", wids[i], entries[i].wid);
    }
    for (int i = 0; i < num_words; i++) {
        int itr = entries[i].itr;
        int wid = entries[i].wid;
        assert(wid == wids[i]);
        maa->spd->setData(my_dst_tile, itr, data[i]);
        my_received_responses++;
    }
    if (state == Status::Response) {
        execute(my_instruction);
    }
}
void StreamAccessUnit::finish(const Fault &fault,
                              const RequestPtr &req, ThreadContext *tc, BaseMMU::Mode mode) {
    assert(fault == NoFault);
    assert(translation_done == false);
    translation_done = true;
    my_translated_physical_address = req->getPaddr();
}

bool RequestTable::add_entry(int itr, Addr base_addr, uint16_t wid) {
    int address_itr = -1;
    int free_address_itr = -1;
    for (int i = 0; i < num_addresses; i++) {
        if (addresses_valid[i] == true) {
            if (addresses[i] == base_addr) {
                address_itr = i;
            }
        } else if (free_address_itr == -1) {
            free_address_itr = i;
        }
    }
    if (address_itr == -1) {
        if (free_address_itr == -1) {
            return false;
        } else {
            addresses[free_address_itr] = base_addr;
            addresses_valid[free_address_itr] = true;
            address_itr = free_address_itr;
        }
    }
    int free_entry_itr = -1;
    for (int i = 0; i < num_entries_per_address; i++) {
        if (entries_valid[address_itr][i] == false) {
            free_entry_itr = i;
            break;
        }
    }
    if (free_entry_itr == -1) {
        return false;
    } else {
        entries[address_itr][free_entry_itr] = RequestTableEntry(itr, wid);
        entries_valid[address_itr][free_entry_itr] = true;
    }
    return true;
}
std::vector<RequestTableEntry> RequestTable::get_entries(Addr base_addr) {
    std::vector<RequestTableEntry> result;
    for (int i = 0; i < num_addresses; i++) {
        if (addresses_valid[i] == true && addresses[i] == base_addr) {
            for (int j = 0; j < num_entries_per_address; j++) {
                if (entries_valid[i][j] == true) {
                    result.push_back(entries[i][j]);
                    entries_valid[i][j] = false;
                }
            }
            addresses_valid[i] = false;
            break;
        }
    }
    return result;
}

} // namespace gem5
