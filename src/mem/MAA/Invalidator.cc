#include "mem/MAA/Invalidator.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "mem/MAA/MAA.hh"
#include "debug/MAAInvalidator.hh"
#include <cassert>

#ifndef TRACING_ON
#define TRACING_ON 1
#endif

namespace gem5 {
Invalidator::Invalidator()
    : executeInstructionEvent([this] { executeInstruction(); }, name()) {
    cl_status = nullptr;
    my_instruction = nullptr;
}
Invalidator::~Invalidator() {
    if (cl_status != nullptr)
        delete[] cl_status;
}
void Invalidator::allocate(int _num_tiles,
                           int _num_tile_elements,
                           Addr _base_addr,
                           MAA *_maa) {
    num_tiles = _num_tiles;
    num_tile_elements = _num_tile_elements;
    my_base_addr = _base_addr;
    maa = _maa;
    total_cls = num_tiles * num_tile_elements * sizeof(uint32_t) / 64;
    cl_status = new CLStatus[total_cls];
    for (int i = 0; i < total_cls; i++) {
        cl_status[i] = CLStatus::Uncached;
    }
    my_instruction = nullptr;
    state = Status::Idle;
}
int Invalidator::get_cl_id(int tile_id, int element_id, int word_size) {
    return (int)((tile_id * num_tile_elements * 4 + element_id * word_size) / 64);
}
void Invalidator::read(int tile_id, int element_id) {
    assert((0 <= tile_id) && (tile_id < num_tiles));
    assert((0 <= element_id) && (element_id < num_tile_elements));
    int cl_id = get_cl_id(tile_id, element_id, 4);
    cl_status[cl_id] = CLStatus::Cached;
    DPRINTF(MAAInvalidator, "%s T[%d] E[%d] CL[%d]: cached\n",
            __func__,
            tile_id,
            element_id,
            cl_id);
}
void Invalidator::setInstruction(Instruction *_instruction) {
    assert(my_instruction == nullptr);
    my_instruction = _instruction;
}
void Invalidator::executeInstruction() {
    switch (state) {
    case Status::Idle: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAInvalidator, "%s: idling %s!\n", __func__, my_instruction->print());
        state = Status::Decode;
        [[fallthrough]];
    }
    case Status::Decode: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAInvalidator, "%s: decoding %s!\n", __func__, my_instruction->print());

        // Decoding the instruction
        panic_if(my_instruction->dst1Ready && my_instruction->dst2Ready,
                 "Both dst1 and dst2 are ready!\n");
        my_dst_tile = my_instruction->dst1Ready == false ? my_instruction->dst1SpdID : my_instruction->dst2SpdID;
        if ((my_instruction->opcode == Instruction::OpcodeType::ALU_SCALAR ||
             my_instruction->opcode == Instruction::OpcodeType::ALU_VECTOR) &&
            (my_instruction->optype == Instruction::OPType::GT_OP ||
             my_instruction->optype == Instruction::OPType::GTE_OP ||
             my_instruction->optype == Instruction::OPType::LT_OP ||
             my_instruction->optype == Instruction::OPType::LTE_OP ||
             my_instruction->optype == Instruction::OPType::EQ_OP)) {
            my_word_size = 4;
        } else {
            my_word_size = my_instruction->getWordSize();
        }

        // Initialization
        my_i = 0;
        my_last_block_addr = 0;
        my_outstanding_pkt = false;
        my_received_responses = 0;
        my_total_invalidations_sent = 0;
        my_decode_start_tick = curTick();
        maa->stats.numInst++;
        maa->stats.numInst_INV++;

        // Setting the state of the instruction and stream unit
        DPRINTF(MAAInvalidator, "%s: state set to request for request %s!\n", __func__, my_instruction->print());
        state = Status::Request;
        [[fallthrough]];
    }
    case Status::Request: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAInvalidator, "%s: requesting %s!\n", __func__, my_instruction->print());
        if (my_outstanding_pkt) {
            if (sendOutstandingPacket() == false) {
                break;
            }
        }
        for (; my_i < num_tile_elements; my_i++) {
            int cl_id = get_cl_id(my_dst_tile, my_i, my_word_size);
            if (cl_status[cl_id] == CLStatus::Cached) {
                DPRINTF(MAAInvalidator, "%s T[%d] E[%d] CL[%d]: cached, invalidating\n",
                        __func__,
                        my_dst_tile,
                        my_i,
                        cl_id);
                Addr curr_block_addr = my_base_addr + cl_id * 64;
                if (curr_block_addr != my_last_block_addr) {
                    my_last_block_addr = curr_block_addr;
                    createMyPacket();
                    my_total_invalidations_sent++;
                    if (sendOutstandingPacket() == false) {
                        my_i++;
                        return;
                    }
                }
            }
        }
        DPRINTF(MAAInvalidator, "%s: state set to respond for request %s!\n", __func__, my_instruction->print());
        state = Status::Response;
        [[fallthrough]];
    }
    case Status::Response: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAInvalidator, "%s: responding %s!\n", __func__, my_instruction->print());
        if (my_received_responses == my_total_invalidations_sent) {
            DPRINTF(MAAInvalidator, "%s: state set to idle for request %s!\n", __func__, my_instruction->print());
            state = Status::Idle;
            maa->setDstReady(my_instruction, my_dst_tile);
            my_instruction = nullptr;
            Cycles total_cycles = maa->getTicksToCycles(curTick() - my_decode_start_tick);
            maa->stats.cycles += total_cycles;
            maa->stats.cycles_INV += total_cycles;
            my_decode_start_tick = 0;
        }
        break;
    }
    default:
        assert(false);
    }
}
void Invalidator::createMyPacket() {
    /**** Packet generation ****/
    RequestPtr real_req = std::make_shared<Request>(
        my_last_block_addr,
        block_size,
        flags,
        maa->requestorId);
    my_pkt = new Packet(real_req, MemCmd::InvalidateReq);
    my_outstanding_pkt = true;
    DPRINTF(MAAInvalidator, "%s: created %s\n", __func__, my_pkt->print());
    (*maa->stats.INV_NumInvalidatedCachelines)++;
}
bool Invalidator::sendOutstandingPacket() {
    DPRINTF(MAAInvalidator, "%s: trying sending %s\n", __func__, my_pkt->print());
    if (maa->cacheSidePort.sendPacket((uint8_t)FuncUnitType::INVALIDATOR,
                                      0,
                                      my_pkt) == false) {
        DPRINTF(MAAInvalidator, "%s: send failed, leaving execution...\n", __func__);
        return false;
    } else {
        my_outstanding_pkt = false;
        return true;
    }
}
void Invalidator::recvData(int tile_id, int element_id) {
    assert((0 <= tile_id) && (tile_id < num_tiles));
    assert((0 <= element_id) && (element_id < num_tile_elements));
    int cl_id = get_cl_id(tile_id, element_id, 4);
    cl_status[cl_id] = CLStatus::Uncached;
    DPRINTF(MAAInvalidator, "%s T[%d] E[%d] CL[%d]: uncached\n",
            __func__,
            tile_id,
            element_id,
            cl_id);
    my_received_responses++;
    if (state == Status::Response && my_received_responses == my_total_invalidations_sent) {
        DPRINTF(MAAInvalidator, "%s: all words received, calling execution again!\n", __func__);
        scheduleExecuteInstructionEvent();
    } else {
        DPRINTF(MAAInvalidator, "%s: expected: %d, received: %d!\n", __func__, my_total_invalidations_sent, my_received_responses);
    }
}
void Invalidator::scheduleExecuteInstructionEvent(int latency) {
    DPRINTF(MAAInvalidator, "%s: scheduling execute for the Invalidator Unit in the next %d cycles!\n", __func__, latency);
    Tick new_when = curTick() + latency;
    panic_if(executeInstructionEvent.scheduled(), "Event already scheduled!\n");
    maa->schedule(executeInstructionEvent, new_when);
    // if (!executeInstructionEvent.scheduled()) {
    //     maa->schedule(executeInstructionEvent, new_when);
    // } else {
    //     Tick old_when = executeInstructionEvent.when();
    //     if (new_when < old_when)
    //         maa->reschedule(executeInstructionEvent, new_when);
    // }
}
} // namespace gem5