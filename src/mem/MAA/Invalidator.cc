#include "mem/MAA/Invalidator.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "mem/MAA/MAA.hh"
#include "mem/MAA/SPD.hh"
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
    // It's possible that the data is cleanevict'ed or clear cleackwirteback'ed and MAA does not know
    // panic_if(cl_status[cl_id] != CLStatus::Uncached, "CL[%d] is not uncached, state: %s!\n",
    //          cl_id, cl_status[cl_id] == CLStatus::ReadCached ? "ReadCached" : "WriteCached");
    cl_status[cl_id] = CLStatus::ReadCached;
    DPRINTF(MAAInvalidator, "%s T[%d] E[%d] CL[%d]: read cached\n",
            __func__,
            tile_id,
            element_id,
            cl_id);
}
void Invalidator::write(int tile_id, int element_id) {
    assert((0 <= tile_id) && (tile_id < num_tiles));
    assert((0 <= element_id) && (element_id < num_tile_elements));
    int cl_id = get_cl_id(tile_id, element_id, 4);
    // It's possible that the data is cleanevict'ed or clear cleackwirteback'ed and MAA does not know
    // panic_if(cl_status[cl_id] != CLStatus::Uncached, "CL[%d] is not uncached, state: %s!\n",
    //          cl_id, cl_status[cl_id] == CLStatus::ReadCached ? "ReadCached" : "WriteCached");
    cl_status[cl_id] = CLStatus::WriteCached;
    DPRINTF(MAAInvalidator, "%s T[%d] E[%d] CL[%d]: write cached\n",
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
        my_invalidating_tile = my_instruction->dst1Status == Instruction::TileStatus::Invalidating ? my_instruction->dst1SpdID : -1;
        my_invalidating_tile = (my_invalidating_tile == -1 && my_instruction->dst2Status == Instruction::TileStatus::Invalidating) ? my_instruction->dst2SpdID : my_invalidating_tile;
        my_invalidating_tile = (my_invalidating_tile == -1 && my_instruction->src1Status == Instruction::TileStatus::Invalidating) ? my_instruction->src1SpdID : my_invalidating_tile;
        my_invalidating_tile = (my_invalidating_tile == -1 && my_instruction->src2Status == Instruction::TileStatus::Invalidating) ? my_instruction->src2SpdID : my_invalidating_tile;
        my_invalidating_tile = (my_invalidating_tile == -1 && my_instruction->condStatus == Instruction::TileStatus::Invalidating) ? my_instruction->condSpdID : my_invalidating_tile;
        panic_if(my_invalidating_tile == -1, "No invalidating tile found!\n");
        my_word_size = my_instruction->getWordSize(my_invalidating_tile);

        // Initialization
        my_i = 0;
        my_last_block_addr = 0;
        my_outstanding_pkt = false;
        my_received_responses = 0;
        my_total_invalidations_sent = 0;
        my_decode_start_tick = curTick();
        maa->stats.numInst++;
        maa->stats.numInst_INV++;
        my_cl_id = -1;

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
            my_cl_id = get_cl_id(my_invalidating_tile, my_i, my_word_size);
            if (cl_status[my_cl_id] == CLStatus::ReadCached || cl_status[my_cl_id] == CLStatus::WriteCached) {
                DPRINTF(MAAInvalidator, "%s T[%d] E[%d] CL[%d]: %s, invalidating\n",
                        __func__, my_invalidating_tile, my_i, my_cl_id, cl_status[my_cl_id] == CLStatus::ReadCached ? "ReadCached" : "WriteCached");
                Addr curr_block_addr = my_base_addr + my_cl_id * 64;
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
            state = Status::Idle;
            maa->finishInstructionInvalidate(my_instruction, my_invalidating_tile);
            DPRINTF(MAAInvalidator, "%s: state set to idle for request %s!\n", __func__, my_instruction->print());
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
    RequestPtr real_req = std::make_shared<Request>(my_last_block_addr, block_size, flags, maa->requestorId);
    my_pkt = new Packet(real_req, MemCmd::ReadExReq);
    my_outstanding_pkt = true;
    my_pkt->allocate();
    my_pkt->setExpressSnoop();
    my_pkt->headerDelay = my_pkt->payloadDelay = 0;
    DPRINTF(MAAInvalidator, "%s: created %s\n", __func__, my_pkt->print());
    (*maa->stats.INV_NumInvalidatedCachelines)++;
}
bool Invalidator::sendOutstandingPacket() {
    DPRINTF(MAAInvalidator, "%s: trying sending %s\n", __func__, my_pkt->print());
    if (maa->sendSnoopInvalidateCpu(my_pkt) == false) {
        DPRINTF(MAAInvalidator, "%s: send failed, leaving send packet...\n", __func__);
        return false;
    }
    if (my_pkt->cacheResponding() == true) {
        DPRINTF(MAAInvalidator, "INV %s: a cache in the O/M state will respond, send successfull...\n", __func__);
    } else if (my_pkt->hasSharers() == true) {
        my_received_responses++;
        cl_status[my_cl_id] = CLStatus::Uncached;
        DPRINTF(MAAInvalidator, "INV %s: There was a cache in the E/S state invalidated\n", __func__);
    } else {
        my_received_responses++;
        cl_status[my_cl_id] = CLStatus::Uncached;
        DPRINTF(MAAInvalidator, "INV %s: no cache responds (I)\n", __func__);
    }
    return true;
}
bool Invalidator::recvData(int tile_id, int element_id, uint8_t *dataptr) {
    assert((0 <= tile_id) && (tile_id < num_tiles));
    assert((0 <= element_id) && (element_id < num_tile_elements));
    int cl_id = get_cl_id(tile_id, element_id, 4);
    assert(cl_status[cl_id] != CLStatus::Uncached);
    cl_status[cl_id] = CLStatus::Uncached;
    DPRINTF(MAAInvalidator, "%s T[%d] E[%d-%d] CL[%d]: uncached\n", __func__, tile_id, element_id, element_id + 15, cl_id);
    my_received_responses++;
    uint32_t *dataptr_u32_typed = (uint32_t *)dataptr;
    for (int i = 0; i < 16; i++) {
        maa->spd->setData<uint32_t>(tile_id, element_id + i, dataptr_u32_typed[i]);
    }
    if (state == Status::Response && my_received_responses == my_total_invalidations_sent) {
        DPRINTF(MAAInvalidator, "%s: all words received, calling execution again!\n", __func__);
        scheduleExecuteInstructionEvent();
    } else {
        DPRINTF(MAAInvalidator, "%s: expected: %d, received: %d!\n", __func__, my_total_invalidations_sent, my_received_responses);
    }
    return true;
}
void Invalidator::scheduleExecuteInstructionEvent(int latency) {
    DPRINTF(MAAInvalidator, "%s: scheduling execute for the Invalidator Unit in the next %d cycles!\n", __func__, latency);
    Tick new_when = maa->getClockEdge(Cycles(latency));
    panic_if(executeInstructionEvent.scheduled(), "Event already scheduled!\n");
    maa->schedule(executeInstructionEvent, new_when);
}
} // namespace gem5