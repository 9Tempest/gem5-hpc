#include "mem/MAA/StreamAccess.hh"
#include "base/trace.hh"
#include "mem/MAA/MAA.hh"
#include "debug/MAAStream.hh"
#include <cassert>

#ifndef TRACING_ON
#define TRACING_ON 1
#endif

namespace gem5 {
///////////////
//
// STREAM ACCESS UNIT
//
///////////////
StreamAccessUnit::StreamAccessUnit()
    : executeInstructionEvent([this] { executeInstruction(); }, name()) {
    request_table = nullptr;
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
    assert(free_entry_itr != -1);
    entries[address_itr][free_entry_itr] = RequestTableEntry(itr, wid);
    entries_valid[address_itr][free_entry_itr] = true;
    return true;
}
void StreamAccessUnit::executeInstruction() {
    switch (state) {
    case Status::Idle: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAStream, "%s: idling %s!\n", __func__, my_instruction->print());
        state = Status::Decode;
        [[fallthrough]];
    }
    case Status::Decode: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAStream, "%s: decoding %s!\n", __func__, my_instruction->print());

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
        my_request_table_full = false;

        // Setting the state of the instruction and stream unit
        DPRINTF(MAAStream, "%s: state set to request for request %s!\n", __func__, my_instruction->print());
        my_instruction->state = Instruction::Status::Service;
        state = Status::Request;
        [[fallthrough]];
    }
    case Status::Request: {
        assert(my_instruction != nullptr);
        assert(my_request_table_full == false);
        DPRINTF(MAAStream, "%s: requesting %s!\n", __func__, my_instruction->print());
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
                if (request_table->add_entry(my_idx, my_translated_physical_address, word_id) == false) {
                    DPRINTF(MAAStream, "RequestTable: entry %d not added! vaddr=0x%lx, paddr=0x%lx wid = %d\n",
                            my_idx, curr_block_addr, my_translated_physical_address, word_id);
                    my_request_table_full = true;
                    return;
                } else {
                    DPRINTF(MAAStream, "RequestTable: entry %d added! vaddr=0x%lx, paddr=0x%lx wid = %d\n",
                            my_idx, curr_block_addr, my_translated_physical_address, word_id);
                }
            }
        }
        if (my_last_block_addr != 0) {
            assert(std::find(my_byte_enable.begin(), my_byte_enable.end(), true) != my_byte_enable.end());
            createMyPacket();
            if (sendOutstandingPacket() == false) {
                break;
            }
        }
        DPRINTF(MAAStream, "%s: state set to respond for request %s!\n", __func__, my_instruction->print());
        state = Status::Response;
        maa->spd->setSize(my_dst_tile, my_idx);
        [[fallthrough]];
    }
    case Status::Response: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAStream, "%s: responding %s!\n", __func__, my_instruction->print());
        if (my_received_responses == my_idx) {
            DPRINTF(MAAStream, "%s: state set to finish for request %s!\n", __func__, my_instruction->print());
            my_instruction->state = Instruction::Status::Finish;
            state = Status::Idle;
            maa->spd->setReady(my_dst_tile);
            maa->finishInstruction(my_instruction, my_dst_tile);
            my_instruction = nullptr;
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
    DPRINTF(MAAStream, "%s: created %s, be:\n", __func__, my_pkt->print());
    for (int j = 0; j < block_size; j++) {
        DPRINTF(MAAStream, "[%d] %s\n", j, my_byte_enable[j] ? "T" : "F");
    }
    DPRINTF(MAAStream, "\n");
    my_last_block_addr = 0;
    std::fill(my_byte_enable.begin(), my_byte_enable.end(), false);
}
bool StreamAccessUnit::sendOutstandingPacket() {
    DPRINTF(MAAStream, "%s: trying sending %s\n", __func__, my_pkt->print());
    if (maa->cacheSidePort.sendPacket(FuncUnitType::STREAM,
                                      my_stream_id,
                                      my_pkt) == false) {
        DPRINTF(MAAStream, "%s: send failed, leaving execution...\n", __func__);
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
bool StreamAccessUnit::recvData(const Addr addr,
                                std::vector<uint32_t> data,
                                std::vector<uint16_t> wids) {
    std::vector<RequestTableEntry> entries = request_table->get_entries(addr);
    if (entries.empty()) {
        DPRINTF(MAAStream, "%s: no entries found for addr(0x%lx)\n", __func__, addr);
        return false;
    }

    int num_words = data.size();
    assert(num_words == wids.size());
    assert(num_words == entries.size());

    DPRINTF(MAAStream, "%s: addr(0x%lx), (RT) wids: \n", __func__, addr);
    for (int i = 0; i < num_words; i++) {
        DPRINTF(MAAStream, " %d | %d\n", wids[i], entries[i].wid);
    }
    for (int i = 0; i < num_words; i++) {
        int itr = entries[i].itr;
        int wid = entries[i].wid;
        assert(wid == wids[i]);
        maa->spd->setData(my_dst_tile, itr, data[i]);
        my_received_responses++;
    }
    if (my_request_table_full) {
        assert(state == Status::Request);
        my_request_table_full = false;
        DPRINTF(MAAStream, "%s: request table was full, calling execution again!\n", __func__);
        scheduleExecuteInstructionEvent();
    } else if (my_received_responses == my_idx) {
        assert(state == Status::Response);
        DPRINTF(MAAStream, "%s: all words received, calling execution again!\n", __func__);
        scheduleExecuteInstructionEvent();
    } else {
        DPRINTF(MAAStream, "%s: expected: %d, received: %d!\n", __func__, my_idx, my_received_responses);
    }
    return true;
}
void StreamAccessUnit::finish(const Fault &fault,
                              const RequestPtr &req, ThreadContext *tc, BaseMMU::Mode mode) {
    assert(fault == NoFault);
    assert(translation_done == false);
    translation_done = true;
    my_translated_physical_address = req->getPaddr();
}
void StreamAccessUnit::setInstruction(Instruction *_instruction) {
    assert(my_instruction == nullptr);
    my_instruction = _instruction;
}
void StreamAccessUnit::scheduleExecuteInstructionEvent(int latency) {
    DPRINTF(MAAStream, "%s: scheduling execute for the next %d cycles!\n", __func__, latency);
    Tick new_when = curTick() + latency;
    if (!executeInstructionEvent.scheduled()) {
        maa->schedule(executeInstructionEvent, new_when);
    } else {
        Tick old_when = executeInstructionEvent.when();
        if (new_when < old_when)
            maa->reschedule(executeInstructionEvent, new_when);
    }
}
} // namespace gem5