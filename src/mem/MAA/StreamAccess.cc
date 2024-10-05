#include "mem/MAA/StreamAccess.hh"
#include "base/types.hh"
#include "mem/MAA/MAA.hh"
#include "mem/MAA/IF.hh"
#include "mem/MAA/SPD.hh"
#include "base/trace.hh"
#include "debug/MAAStream.hh"
#include "sim/cur_tick.hh"
#include <cassert>

#ifndef TRACING_ON
#define TRACING_ON 1
#endif

namespace gem5 {

///////////////
// REQUEST TABLE
///////////////
RequestTable::RequestTable(StreamAccessUnit *_stream_access, int _my_stream_id) {
    stream_access = _stream_access;
    my_stream_id = _my_stream_id;
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
RequestTable::~RequestTable() {
    for (int i = 0; i < num_addresses; i++) {
        delete[] entries[i];
        delete[] entries_valid[i];
    }
    delete[] entries;
    delete[] entries_valid;
    delete[] addresses;
    delete[] addresses_valid;
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
    assert(result.size() > 0);
    return result;
}
bool RequestTable::add_entry(int itr, Addr base_addr, uint16_t wid) {
    int address_itr = -1;
    int free_address_itr = -1;
    for (int i = 0; i < num_addresses; i++) {
        if (addresses_valid[i] == true) {
            if (addresses[i] == base_addr) {
                // Duplicate should not be allowed
                assert(address_itr == -1);
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
            (*stream_access->maa->stats.STR_NumCacheLineInserted[my_stream_id])++;
        }
    }
    int free_entry_itr = -1;
    for (int i = 0; i < num_entries_per_address; i++) {
        if (entries_valid[address_itr][i] == false) {
            free_entry_itr = i;
            break;
        }
    }
    assert(free_entry_itr != -1);
    entries[address_itr][free_entry_itr] = RequestTableEntry(itr, wid);
    entries_valid[address_itr][free_entry_itr] = true;
    (*stream_access->maa->stats.STR_NumWordsInserted[my_stream_id])++;
    return true;
}
void RequestTable::check_reset() {
    for (int i = 0; i < num_addresses; i++) {
        panic_if(addresses_valid[i], "Address %d is valid: 0x%lx!\n", i, addresses[i]);
        for (int j = 0; j < num_entries_per_address; j++) {
            panic_if(entries_valid[i][j], "Entry %d is valid: itr(%u) wid(%u)!\n", j, entries[i][j].itr, entries[i][j].wid);
        }
    }
}
void RequestTable::reset() {
    for (int i = 0; i < num_addresses; i++) {
        addresses_valid[i] = false;
        for (int j = 0; j < num_entries_per_address; j++) {
            entries_valid[i][j] = false;
        }
    }
}
bool RequestTable::is_full() {
    for (int i = 0; i < num_addresses; i++) {
        if (addresses_valid[i] == false) {
            return false;
        }
    }
    return true;
}

///////////////
//
// STREAM ACCESS UNIT
//
///////////////
StreamAccessUnit::StreamAccessUnit()
    : executeInstructionEvent([this] { executeInstruction(); }, name()),
      sendPacketEvent([this] { sendOutstandingReadPacket(); }, name()) {
    request_table = nullptr;
    my_instruction = nullptr;
}
void StreamAccessUnit::allocate(int _my_stream_id, unsigned int _num_tile_elements, MAA *_maa) {
    my_stream_id = _my_stream_id;
    num_tile_elements = _num_tile_elements;
    state = Status::Idle;
    maa = _maa;
    dst_tile_id = -1;
    request_table = new RequestTable(this, my_stream_id);
    my_translation_done = false;
    my_instruction = nullptr;
}
bool StreamAccessUnit::scheduleNextExecution(bool force) {
    Tick finish_tick = std::max(my_SPD_read_finish_tick, my_SPD_write_finish_tick);
    finish_tick = std::max(finish_tick, my_RT_access_finish_tick);
    if (curTick() < finish_tick) {
        scheduleExecuteInstructionEvent(maa->getTicksToCycles(finish_tick - curTick()));
        return true;
    } else if (force) {
        scheduleExecuteInstructionEvent(Cycles(0));
        return true;
    }
    return false;
}
bool StreamAccessUnit::scheduleNextSend() {
    if (my_outstanding_read_pkts.size() > 0) {
        Cycles latency = Cycles(0);
        if (my_outstanding_read_pkts.begin()->tick > curTick()) {
            latency = maa->getTicksToCycles(my_outstanding_read_pkts.begin()->tick - curTick());
        }
        scheduleSendPacketEvent(latency);
        return true;
    }
    return false;
}
void StreamAccessUnit::executeInstruction() {
    switch (state) {
    case Status::Idle: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAStream, "S[%d] %s: idling %s!\n", my_stream_id, __func__, my_instruction->print());
        state = Status::Decode;
        [[fallthrough]];
    }
    case Status::Decode: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAStream, "S[%d] %s: decoding %s!\n", my_stream_id, __func__, my_instruction->print());

        // Decoding the instruction
        my_dst_tile = my_instruction->dst1SpdID;
        my_cond_tile = my_instruction->condSpdID;
        my_min = maa->rf->getData<int>(my_instruction->src1RegID);
        my_max = maa->rf->getData<int>(my_instruction->src2RegID);
        my_stride = maa->rf->getData<int>(my_instruction->src3RegID);
        my_word_size = my_instruction->getWordSize();
        my_words_per_cl = 64 / my_word_size;
        (*maa->stats.STR_NumInsts[my_stream_id])++;
        maa->stats.numInst_STRRD++;
        maa->stats.numInst++;

        // Initialization
        my_i = my_min;
        my_idx = 0;
        my_base_addr = my_instruction->baseAddr;
        my_received_responses = 0;
        my_sent_requests = 0;
        request_table->reset();
        my_last_block_vaddr = 0;
        my_SPD_read_finish_tick = curTick();
        my_SPD_write_finish_tick = curTick();
        my_RT_access_finish_tick = curTick();
        my_decode_start_tick = curTick();
        my_request_start_tick = 0;

        // Setting the state of the instruction and stream unit
        my_instruction->state = Instruction::Status::Service;
        state = Status::Request;
        [[fallthrough]];
    }
    case Status::Request: {
        DPRINTF(MAAStream, "S[%d] %s: filling %s!\n", my_stream_id, __func__, my_instruction->print());
        if (scheduleNextExecution() || request_table->is_full()) {
            break;
        }
        if (my_request_start_tick == 0) {
            my_request_start_tick = curTick();
        }
        int num_spd_read_accesses = 0;
        int num_request_table_word_accesses = 0;
        int num_request_table_cacheline_accesses = 0;
        for (; my_i < my_max && my_idx < maa->num_tile_elements; my_i += my_stride, my_idx++) {
            if (my_cond_tile != -1) {
                num_spd_read_accesses++;
            }
            if (my_cond_tile == -1 || maa->spd->getData<uint32_t>(my_cond_tile, my_idx) != 0) {
                Addr vaddr = my_base_addr + my_word_size * my_i;
                Addr block_vaddr = addrBlockAlign(vaddr, block_size);
                if (block_vaddr != my_last_block_vaddr) {
                    if (my_last_block_vaddr != 0) {
                        my_sent_requests++;
                        Addr paddr = translatePacket(my_last_block_vaddr);
                        createReadPacket(paddr, num_request_table_word_accesses);
                        num_request_table_cacheline_accesses++;
                    }
                    my_last_block_vaddr = block_vaddr;
                }
                Addr paddr = translatePacket(block_vaddr);
                uint16_t word_id = (vaddr - block_vaddr) / my_word_size;
                if (request_table->add_entry(my_idx, paddr, word_id) == false) {
                    DPRINTF(MAAStream, "S[%d] RequestTable: entry %d not added! vaddr=0x%lx, paddr=0x%lx wid = %d\n",
                            my_stream_id, my_idx, block_vaddr, paddr, word_id);

                    // 4Byte conditions -- 16 bytes per SPD access
                    num_spd_read_accesses = (num_spd_read_accesses + 15) / 16;
                    Cycles spd_read_accesses_latency = maa->spd->getDataLatency(num_spd_read_accesses);
                    my_SPD_read_finish_tick = maa->getClockEdge(spd_read_accesses_latency);
                    (*maa->stats.STR_CyclesSPDReadAccess[my_stream_id]) += spd_read_accesses_latency;

                    my_RT_access_finish_tick = maa->getClockEdge(Cycles(num_request_table_cacheline_accesses));
                    (*maa->stats.STR_CyclesRTAccess[my_stream_id]) += num_request_table_cacheline_accesses;

                    scheduleNextExecution();
                    scheduleNextSend();
                    (*maa->stats.STR_NumDrains[my_stream_id])++;
                    return;
                } else {
                    num_request_table_word_accesses++;
                    DPRINTF(MAAStream, "S[%d] RequestTable: entry %d added! vaddr=0x%lx, paddr=0x%lx wid = %d\n",
                            my_stream_id, my_idx, block_vaddr, paddr, word_id);
                }
            }
        }
        if (my_last_block_vaddr != 0) {
            my_sent_requests++;
            Addr paddr = translatePacket(my_last_block_vaddr);
            createReadPacket(paddr, num_request_table_word_accesses);
            my_last_block_vaddr = 0;
        }
        // spd read is words -- 4Byte conditions -- 16 bytes per SPD access
        num_spd_read_accesses = (num_spd_read_accesses + 15) / 16;
        Cycles spd_read_accesses_latency = maa->spd->getDataLatency(num_spd_read_accesses);
        my_SPD_read_finish_tick = maa->getClockEdge(spd_read_accesses_latency);
        (*maa->stats.STR_CyclesSPDReadAccess[my_stream_id]) += spd_read_accesses_latency;

        my_RT_access_finish_tick = maa->getClockEdge(Cycles(num_request_table_cacheline_accesses));
        (*maa->stats.STR_CyclesRTAccess[my_stream_id]) += num_request_table_cacheline_accesses;

        scheduleNextExecution();
        scheduleNextSend();
        if (my_received_responses != my_sent_requests) {
            DPRINTF(MAAStream, "S[%d] %s: Waiting for responses, received (%d) != send (%d)...\n", my_stream_id, __func__, my_received_responses, my_sent_requests);
            break;
        }
        DPRINTF(MAAStream, "S[%d] %s: state set to respond for request %s!\n", my_stream_id, __func__, my_instruction->print());
        state = Status::Response;
        [[fallthrough]];
    }
    case Status::Response: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAStream, "S[%d] %s: responding %s!\n", my_stream_id, __func__, my_instruction->print());
        panic_if(scheduleNextExecution(), "S[%d] %s: Execution is not completed!\n", my_stream_id, __func__);
        panic_if(scheduleNextSend(), "S[%d] %s: Sending is not completed!\n", my_stream_id, __func__);
        panic_if(my_received_responses != my_sent_requests, "S[%d] %s: received_responses(%d) != sent_requests(%d)!\n",
                 my_stream_id, __func__, my_received_responses, my_sent_requests);
        DPRINTF(MAAStream, "S[%d] %s: state set to finish for request %s!\n", my_stream_id, __func__, my_instruction->print());
        my_instruction->state = Instruction::Status::Finish;
        if (my_request_start_tick != 0) {
            (*maa->stats.STR_CyclesRequest[my_stream_id]) += maa->getTicksToCycles(curTick() - my_request_start_tick);
            my_request_start_tick = 0;
        }
        Cycles total_cycles = maa->getTicksToCycles(curTick() - my_decode_start_tick);
        maa->stats.cycles += total_cycles;
        maa->stats.cycles_STRRD += total_cycles;
        my_decode_start_tick = 0;
        state = Status::Idle;
        maa->spd->setSize(my_dst_tile, my_idx);
        maa->setTileReady(my_dst_tile);
        if (my_word_size == 8) {
            maa->setTileReady(my_dst_tile + 1);
        }
        maa->finishInstruction(my_instruction, my_dst_tile);
        my_instruction = nullptr;
        request_table->check_reset();
        break;
    }
    default:
        assert(false);
    }
}
void StreamAccessUnit::createReadPacket(Addr addr, int latency) {
    /**** Packet generation ****/
    RequestPtr real_req = std::make_shared<Request>(addr, block_size, flags, maa->requestorId);
    PacketPtr my_pkt = new Packet(real_req, MemCmd::ReadSharedReq);
    my_pkt->allocate();
    my_outstanding_read_pkts.insert(StreamAccessUnit::StreamPacket(my_pkt, maa->getClockEdge(Cycles(latency))));
    DPRINTF(MAAStream, "S[%d] %s: created %s to send in %d cycles\n", my_stream_id, __func__, my_pkt->print(), latency);
    (*maa->stats.STR_LoadsCacheAccessing[my_stream_id])++;
}
void StreamAccessUnit::createReadPacketEvict(Addr addr) {
    /**** Packet generation ****/
    RequestPtr real_req = std::make_shared<Request>(addr, block_size, flags, maa->requestorId);
    PacketPtr my_pkt = new Packet(real_req, MemCmd::CleanEvict);
    my_outstanding_read_pkts.insert(StreamAccessUnit::StreamPacket(my_pkt, maa->getClockEdge(Cycles(0))));
    DPRINTF(MAAStream, "S[%d] %s: created %s to send in %d cycles\n", my_stream_id, __func__, my_pkt->print(), 0);
    (*maa->stats.STR_Evicts[my_stream_id])++;
}
bool StreamAccessUnit::sendOutstandingReadPacket() {
    bool packet_sent = false;
    DPRINTF(MAAStream, "S[%d] %s: sending %d outstanding read packets...\n", my_stream_id, __func__, my_outstanding_read_pkts.size());
    while (my_outstanding_read_pkts.empty() == false) {
        StreamAccessUnit::StreamPacket read_pkt = *my_outstanding_read_pkts.begin();
        DPRINTF(MAAStream, "S[%d] %s: trying sending %s to cache at time %u\n",
                my_stream_id, __func__, read_pkt.packet->print(), read_pkt.tick);
        if (read_pkt.tick > curTick()) {
            scheduleNextSend();
            return false;
        }
        if (maa->cacheSidePort.sendPacket((uint8_t)FuncUnitType::STREAM,
                                          my_stream_id,
                                          read_pkt.packet) == false) {
            DPRINTF(MAAStream, "S[%d] %s: send failed, leaving execution...\n", my_stream_id, __func__);
            return false;
        } else {
            my_outstanding_read_pkts.erase(my_outstanding_read_pkts.begin());
            packet_sent = true;
        }
    }
    if (packet_sent && (my_received_responses == my_sent_requests)) {
        scheduleNextExecution(true);
    } else {
        DPRINTF(MAAStream, "S[%d] %s: expected: %d, received: %d, packet send: %d!\n", my_stream_id, __func__, my_sent_requests, my_received_responses, packet_sent);
    }
    return true;
}
bool StreamAccessUnit::recvData(const Addr addr,
                                uint8_t *dataptr) {
    bool was_request_table_full = request_table->is_full();
    std::vector<RequestTableEntry> entries = request_table->get_entries(addr);
    if (entries.empty()) {
        DPRINTF(MAAStream, "S[%d] %s: no entries found for addr(0x%lx)\n", my_stream_id, __func__, addr);
        return false;
    }
    DPRINTF(MAAStream, "S[%d] %s: %d entry found for addr(0x%lx)\n", my_stream_id, __func__, entries.size(), addr);
    uint32_t *dataptr_u32_typed = (uint32_t *)dataptr;
    uint64_t *dataptr_u64_typed = (uint64_t *)dataptr;
    for (auto entry : entries) {
        int itr = entry.itr;
        int wid = entry.wid;
        if (my_word_size == 4) {
            DPRINTF(MAAStream, "S[%d] %s: SPD[%d][%d] = %u\n", my_stream_id, __func__, my_dst_tile, itr, dataptr_u32_typed[wid]);
            maa->spd->setData<uint32_t>(my_dst_tile, itr, dataptr_u32_typed[wid]);
        } else {
            DPRINTF(MAAStream, "S[%d] %s: SPD[%d][%d] = %lu\n", my_stream_id, __func__, my_dst_tile, itr, dataptr_u64_typed[wid]);
            maa->spd->setData<uint64_t>(my_dst_tile, itr, dataptr_u64_typed[wid]);
        }
    }
    my_received_responses++;

    // XByte -- 64/X bytes per SPD access
    int num_spd_write_accesses = (entries.size() + my_words_per_cl - 1) / my_words_per_cl;
    Cycles access_spd_latency = maa->spd->setDataLatency(num_spd_write_accesses);
    (*maa->stats.STR_CyclesSPDWriteAccess[my_stream_id]) += access_spd_latency;
    my_SPD_write_finish_tick = maa->getClockEdge(access_spd_latency);

    // this is 1 because we need only 1 access to get all words of a cacheline
    Cycles access_rt_latency = Cycles(1); // Cycles(entries.size());
    (*maa->stats.STR_CyclesRTAccess[my_stream_id]) += access_rt_latency;
    if (my_RT_access_finish_tick < curTick())
        my_RT_access_finish_tick = maa->getClockEdge(access_rt_latency);
    else
        my_RT_access_finish_tick += maa->getCyclesToTicks(access_rt_latency);

    createReadPacketEvict(addr);
    scheduleNextSend();
    if (was_request_table_full) {
        scheduleNextExecution(true);
    }
    return true;
}
Addr StreamAccessUnit::translatePacket(Addr vaddr) {
    /**** Address translation ****/
    RequestPtr translation_req = std::make_shared<Request>(vaddr,
                                                           block_size,
                                                           flags,
                                                           maa->requestorId,
                                                           my_instruction->PC,
                                                           my_instruction->CID);
    ThreadContext *tc = maa->system->threads[my_instruction->CID];
    maa->mmu->translateTiming(translation_req, tc, this, BaseMMU::Read);
    // The above function immediately does the translation and calls the finish function
    assert(my_translation_done);
    my_translation_done = false;
    return my_translated_addr;
}
void StreamAccessUnit::finish(const Fault &fault,
                              const RequestPtr &req, ThreadContext *tc, BaseMMU::Mode mode) {
    assert(fault == NoFault);
    assert(my_translation_done == false);
    my_translation_done = true;
    my_translated_addr = req->getPaddr();
}
void StreamAccessUnit::setInstruction(Instruction *_instruction) {
    assert(my_instruction == nullptr);
    my_instruction = _instruction;
}
void StreamAccessUnit::scheduleExecuteInstructionEvent(int latency) {
    DPRINTF(MAAStream, "S[%d] %s: scheduling execute for the Stream Unit in the next %d cycles!\n", my_stream_id, __func__, latency);
    panic_if(latency < 0, "Negative latency of %d!\n", latency);
    Tick new_when = maa->getClockEdge(Cycles(latency));
    if (!executeInstructionEvent.scheduled()) {
        maa->schedule(executeInstructionEvent, new_when);
    } else {
        Tick old_when = executeInstructionEvent.when();
        DPRINTF(MAAStream, "S[%d] %s: execution already scheduled for tick %d\n", my_stream_id, __func__, old_when);
        if (new_when < old_when) {
            DPRINTF(MAAStream, "S[%d] %s: rescheduling for tick %d!\n", my_stream_id, __func__, new_when);
            maa->reschedule(executeInstructionEvent, new_when);
        }
    }
}
void StreamAccessUnit::scheduleSendPacketEvent(int latency) {
    DPRINTF(MAAStream, "S[%d] %s: scheduling send packet for the Stream Unit in the next %d cycles!\n", my_stream_id, __func__, latency);
    panic_if(latency < 0, "Negative latency of %d!\n", latency);
    Tick new_when = maa->getClockEdge(Cycles(latency));
    if (!sendPacketEvent.scheduled()) {
        maa->schedule(sendPacketEvent, new_when);
    } else {
        Tick old_when = sendPacketEvent.when();
        DPRINTF(MAAStream, "S[%d] %s: send packet already scheduled for tick %d\n", my_stream_id, __func__, old_when);
        if (new_when < old_when) {
            DPRINTF(MAAStream, "S[%d] %s: rescheduling for tick %d!\n", my_stream_id, __func__, new_when);
            maa->reschedule(sendPacketEvent, new_when);
        }
    }
}
} // namespace gem5