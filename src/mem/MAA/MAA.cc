#include "mem/MAA/MAA.hh"
#include "base/addr_range.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "mem/MAA/IndirectAccess.hh"
#include "mem/packet.hh"
#include "params/MAA.hh"
#include "debug/MAA.hh"
#include "debug/MAACpuPort.hh"
#include "debug/MAACachePort.hh"
#include "debug/MAAMemPort.hh"
#include "debug/MAAController.hh"
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
      num_row_table_rows(p.num_row_table_rows),
      num_row_table_entries_per_row(p.num_row_table_entries_per_row),
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
    indirectAccessUnits = new IndirectAccessUnit[num_indirect_access_units];
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
void MAA::addRamulator(memory::Ramulator2 *_ramulator2) {
    _ramulator2->getAddrMapData(m_org,
                                m_addr_bits,
                                m_num_levels,
                                m_tx_offset,
                                m_col_bits_idx,
                                m_row_bits_idx);
    DPRINTF(MAA, "Ramulator organization [n_levels: %d] -- CH: %d, RA: %d, BG: %d, BA: %d, RO: %d, CO: %d\n",
            m_num_levels,
            m_org[ADDR_CHANNEL_LEVEL],
            m_org[ADDR_RANK_LEVEL],
            m_org[ADDR_BANKGROUP_LEVEL],
            m_org[ADDR_BANK_LEVEL],
            m_org[ADDR_ROW_LEVEL],
            m_org[ADDR_COLUMN_LEVEL]);
    assert(m_num_levels == 6);
    for (int i = 0; i < num_indirect_access_units; i++) {
        indirectAccessUnits[i].allocate(num_tile_elements,
                                        num_row_table_rows,
                                        num_row_table_entries_per_row,
                                        this);
    }
}
// RoBaRaCoCh address mapping taking from the Ramulator2
int slice_lower_bits(uint64_t &addr, int bits) {
    int lbits = addr & ((1 << bits) - 1);
    addr >>= bits;
    return lbits;
}
std::vector<int> MAA::map_addr(Addr addr) {
    std::vector<int> addr_vec(m_num_levels, -1);
    addr = addr >> m_tx_offset;
    addr_vec[0] = slice_lower_bits(addr, m_addr_bits[0]);
    addr_vec[m_addr_bits.size() - 1] = slice_lower_bits(addr, m_addr_bits[m_addr_bits.size() - 1]);
    for (int i = 1; i <= m_row_bits_idx; i++) {
        addr_vec[i] = slice_lower_bits(addr, m_addr_bits[i]);
    }
    return addr_vec;
}
Addr MAA::calc_Grow_addr(std::vector<int> addr_vec) {
    assert(addr_vec.size() == 6);
    Addr Grow_addr = (addr_vec[ADDR_BANKGROUP_LEVEL] >> 1) * m_org[ADDR_BANK_LEVEL];
    Grow_addr = (Grow_addr + addr_vec[ADDR_BANK_LEVEL]) * m_org[ADDR_ROW_LEVEL];
    Grow_addr += addr_vec[ADDR_ROW_LEVEL];
    return Grow_addr;
}
///////////////
//
// CpuSidePort
//
///////////////
void MAA::recvTimingSnoopResp(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACpuPort, "%s: received %s\n", __func__, pkt->print());
    assert(false);
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
    assert(false);
    // Express snoop responses from requestor to responder, e.g., from L1 to L2
    maa.recvTimingSnoopResp(pkt);
    return true;
}

bool MAA::CpuSidePort::tryTiming(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACpuPort, "%s: received %s\n", __func__, pkt->print());
    return true;
}

void MAA::recvTimingReq(PacketPtr pkt) {
    /// print the packet
    AddressRangeType address_range = AddressRangeType(pkt->getAddr(), addrRanges);
    DPRINTF(MAACpuPort, "%s: received %s, %s, cmd: %s, isMaskedWrite: %d, size: %d\n",
            __func__,
            pkt->print(),
            address_range.print(),
            pkt->cmdString(),
            pkt->isMaskedWrite(),
            pkt->getSize());
    for (int i = 0; i < pkt->getSize(); i++) {
        DPRINTF(MAACpuPort, "%02x %s\n", pkt->getPtr<uint8_t>()[i], pkt->req->getByteEnable()[i] ? "True" : "False");
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
            DPRINTF(MAAController, "%s: TILE[%d][%d] = %d\n", __func__, tile_id, element_id, data);
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
            DPRINTF(MAAController, "%s: REG[%d] = %d\n", __func__, element_id, data);
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
            DPRINTF(MAAController, "%s: IF[%d] = %ld\n", __func__, element_id, data);
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
                current_instruction.src1Ready = current_instruction.src1SpdID == -1 ? true : spd->getReady(current_instruction.src1SpdID);
                current_instruction.src2Ready = current_instruction.src2SpdID == -1 ? true : spd->getReady(current_instruction.src2SpdID);
                DPRINTF(MAAController, "%s: %s pushing!\n", __func__, current_instruction.print());
                assert(ifile->pushInstruction(current_instruction));
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
                DPRINTF(MAAController, "%s: %s pushed!\n", __func__, current_instruction.print());
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
            DPRINTF(MAAController, "%s: Error: Range(%s) and cmd(%s) is illegal\n",
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
    DPRINTF(MAAMemPort, "%s: received %s, cmd: %s, size: %d\n",
            __func__,
            pkt->print(),
            pkt->cmdString(),
            pkt->getSize());
    for (int i = 0; i < pkt->getSize(); i++) {
        DPRINTF(MAAMemPort, "%02x %s\n", pkt->getPtr<uint8_t>()[i], pkt->req->getByteEnable()[i] ? "True" : "False");
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
        for (int i = 0; i < num_indirect_access_units; i++) {
            if (indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Request ||
                indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Response) {
                indirectAccessUnits[i].recvData(pkt->getAddr(), data, wid, false);
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
    // this will be used for the indirect access
    for (int i = 0; i < maa->num_indirect_access_units; i++) {
        if (maa->indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Request) {
            maa->indirectAccessUnits[i].execute();
        }
    }
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
}

///////////////
//
// CacheSidePort
//
///////////////
void MAA::recvCacheTimingResp(PacketPtr pkt) {
    /// print the packet
    DPRINTF(MAACachePort, "%s: received %s, cmd: %s, size: %d\n",
            __func__,
            pkt->print(),
            pkt->cmdString(),
            pkt->getSize());
    for (int i = 0; i < pkt->getSize(); i++) {
        DPRINTF(MAACachePort, "%02x %s\n", pkt->getPtr<uint8_t>()[i], pkt->req->getByteEnable()[i] ? "True" : "False");
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
        for (int i = 0; i < num_indirect_access_units; i++) {
            if (indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Request ||
                indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Response) {
                indirectAccessUnits[i].recvData(pkt->getAddr(), data, wid, true);
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
    DPRINTF(MAACachePort, "%s: received %s\n", __func__, pkt->print());
    maa->recvCacheTimingResp(pkt);
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
    for (int i = 0; i < maa->num_stream_access_units; i++) {
        if (maa->streamAccessUnits[i].getState() == StreamAccessUnit::Status::Request) {
            maa->streamAccessUnits[i].execute();
        }
    }
    for (int i = 0; i < maa->num_indirect_access_units; i++) {
        if (maa->indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Request) {
            maa->indirectAccessUnits[i].execute();
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
    for (int i = 0; i < num_indirect_access_units; i++) {
        if (indirectAccessUnits[i].getState() == IndirectAccessUnit::Status::Idle) {
            Instruction *inst = ifile->getReady(Instruction::FuncUnitType::INDIRECT);
            if (inst != nullptr) {
                indirectAccessUnits[i].execute(inst);
            }
        }
    }
}
void MAA::finishInstruction(Instruction *instruction,
                            int dst1SpdID,
                            int dst2SpdID) {
    DPRINTF(MAAController, "%s: %s finishing!\n", __func__, instruction->print());
    if (dst1SpdID != -1) {
        spd->setReady(dst1SpdID);
    }
    if (dst2SpdID != -1) {
        spd->setReady(dst2SpdID);
    }
    ifile->finishInstruction(instruction, dst1SpdID, dst2SpdID);
    issueInstruction();
}

} // namespace gem5
