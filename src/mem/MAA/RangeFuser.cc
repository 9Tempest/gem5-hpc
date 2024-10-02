#include "mem/MAA/RangeFuser.hh"
#include "base/types.hh"
#include "mem/MAA/MAA.hh"
#include "mem/MAA/IF.hh"
#include "mem/MAA/SPD.hh"
#include "base/trace.hh"
#include "debug/MAARangeFuser.hh"
#include <cassert>

#ifndef TRACING_ON
#define TRACING_ON 1
#endif

namespace gem5 {
///////////////
//
// ALU ACCESS UNIT
//
///////////////
RangeFuserUnit::RangeFuserUnit()
    : executeInstructionEvent([this] { executeInstruction(); }, name()) {
    my_instruction = nullptr;
}
void RangeFuserUnit::allocate(unsigned int _num_tile_elements, MAA *_maa, int _my_range_id) {
    state = Status::Idle;
    num_tile_elements = _num_tile_elements;
    maa = _maa;
    my_range_id = _my_range_id;
    my_instruction = nullptr;
}
void RangeFuserUnit::executeInstruction() {
    switch (state) {
    case Status::Idle: {
        assert(my_instruction != nullptr);
        DPRINTF(MAARangeFuser, "%s: idling %s!\n", __func__, my_instruction->print());
        state = Status::Decode;
        [[fallthrough]];
    }
    case Status::Decode: {
        assert(my_instruction != nullptr);
        DPRINTF(MAARangeFuser, "%s: decoding %s!\n", __func__, my_instruction->print());

        // Decoding the instruction
        my_dst_i_tile = my_instruction->dst1SpdID;
        my_dst_j_tile = my_instruction->dst2SpdID;
        my_cond_tile = my_instruction->condSpdID;
        my_min_tile = my_instruction->src1SpdID;
        my_max_tile = my_instruction->src2SpdID;
        my_last_i = maa->rf->getData<int>(my_instruction->dst1RegID);
        my_last_j = maa->rf->getData<int>(my_instruction->dst2RegID);
        my_stride = maa->rf->getData<int>(my_instruction->src1RegID);
        my_max_i = maa->spd->getSize(my_min_tile);
        my_idx_j = 0;
        panic_if(my_max_i != maa->spd->getSize(my_max_tile),
                 "%s: min tile size(%d) != max tile size(%d)!\n",
                 __func__, my_max_i, maa->spd->getSize(my_max_tile));
        panic_if(my_cond_tile != -1 && my_max_i != maa->spd->getSize(my_cond_tile),
                 "%s: min tile size(%d) != cond size(%d)!\n",
                 __func__, my_max_i, maa->spd->getSize(my_cond_tile));
        DPRINTF(MAARangeFuser, "%s: my_last_i: %d, my_last_j: %d, my_stride: %d, my_max_i: %d, my_idx_j: %d\n",
                __func__, my_last_i, my_last_j, my_stride, my_max_i, my_idx_j);
        maa->stats.numInst_RANGE++;
        (*maa->stats.RNG_NumInsts[my_range_id])++;
        maa->stats.numInst++;

        // Setting the state of the instruction and ALU unit
        DPRINTF(MAARangeFuser, "%s: state set to work for request %s!\n", __func__, my_instruction->print());
        my_instruction->state = Instruction::Status::Service;
        state = Status::Work;
        [[fallthrough]];
    }
    case Status::Work: {
        assert(my_instruction != nullptr);
        DPRINTF(MAARangeFuser, "%s: working %s!\n", __func__, my_instruction->print());
        int num_spd_read_accesses = 0;
        int num_spd_write_accesses = 0;
        for (; my_last_i < my_max_i && my_idx_j < num_tile_elements; my_last_i++) {
            if (my_cond_tile != -1) {
                num_spd_read_accesses++;
            }
            if (my_cond_tile == -1 || maa->spd->getData<uint32_t>(my_cond_tile, my_last_i) != 0) {
                if (my_last_j == -1) {
                    uint32_t my_min_j = maa->spd->getData<uint32_t>(my_min_tile, my_last_i);
                    num_spd_read_accesses++;
                    my_last_j = my_min_j;
                }
                uint32_t my_max_j = maa->spd->getData<uint32_t>(my_max_tile, my_last_i);
                num_spd_read_accesses++;
                for (; my_last_j < my_max_j &&
                       my_idx_j < num_tile_elements;
                     my_last_j += my_stride, my_idx_j++) {
                    maa->spd->setData(my_dst_i_tile, my_idx_j, my_last_i);
                    maa->spd->setData(my_dst_j_tile, my_idx_j, my_last_j);
                    num_spd_write_accesses += 2;
                    DPRINTF(MAARangeFuser, "%s: [%d][%d] inserted!\n", __func__, my_last_i, my_last_j);
                }
                if (my_last_j >= my_max_j) {
                    my_last_j = -1;
                } else if (my_idx_j == num_tile_elements) {
                    break;
                }
            }
        }
        Cycles spd_read_latency = maa->spd->getDataLatency(num_spd_read_accesses);
        (*maa->stats.RNG_CyclesSPDReadAccess[my_range_id]) += spd_read_latency;
        Cycles spd_write_latency = maa->spd->setDataLatency(num_spd_write_accesses);
        (*maa->stats.RNG_CyclesSPDWriteAccess[my_range_id]) += spd_write_latency;
        Cycles compute_latency = Cycles(my_idx_j);
        (*maa->stats.RNG_CyclesCompute[my_range_id]) += compute_latency;
        Cycles final_latency = std::max(spd_read_latency, std::max(spd_write_latency, compute_latency));
        maa->stats.cycles_RANGE += final_latency;
        maa->stats.cycles += final_latency;
        DPRINTF(MAARangeFuser, "%s: setting state to finish for request %s in %d cycles!\n", __func__, my_instruction->print(), final_latency);
        state = Status::Finish;
        scheduleExecuteInstructionEvent(final_latency);
        break;
    }
    case Status::Finish: {
        DPRINTF(MAARangeFuser, "%s: finishing %s!\n", __func__, my_instruction->print());
        my_instruction->state = Instruction::Status::Finish;
        state = Status::Idle;
        maa->rf->setData<int>(my_instruction->dst1RegID, my_last_i);
        maa->rf->setData<int>(my_instruction->dst2RegID, my_last_j);
        int tile_size = my_idx_j; // my_idx_j == -1 ? num_tile_elements : my_idx_j;
        maa->spd->setSize(my_dst_i_tile, tile_size);
        maa->spd->setSize(my_dst_j_tile, tile_size);
        maa->setTileReady(my_dst_i_tile);
        maa->setTileReady(my_dst_j_tile);
        maa->finishInstruction(my_instruction, my_dst_i_tile, my_dst_j_tile);
        DPRINTF(MAARangeFuser, "%s: my_last_i: %d [REG %d], my_last_j: %d [REG %d], my_idx_j: %d, tile size: %d\n",
                __func__,
                my_last_i, my_instruction->dst1RegID,
                my_last_j, my_instruction->dst2RegID,
                my_idx_j, tile_size);
        my_instruction = nullptr;
        break;
    }
    default:
        assert(false);
    }
}
void RangeFuserUnit::setInstruction(Instruction *_instruction) {
    assert(my_instruction == nullptr);
    my_instruction = _instruction;
}
void RangeFuserUnit::scheduleExecuteInstructionEvent(int latency) {
    DPRINTF(MAARangeFuser, "%s: scheduling execute for the RangeFuser Unit in the next %d cycles!\n", __func__, latency);
    Tick new_when = maa->getClockEdge(Cycles(latency));
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