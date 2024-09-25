#include "mem/MAA/ALU.hh"
#include "mem/MAA/MAA.hh"
#include "mem/MAA/IF.hh"
#include "mem/MAA/SPD.hh"
#include "base/trace.hh"
#include "debug/MAAALU.hh"
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
ALUUnit::ALUUnit()
    : executeInstructionEvent([this] { executeInstruction(); }, name()) {
    my_dst_tile = -1;
    my_instruction = nullptr;
}
void ALUUnit::allocate(MAA *_maa, Cycles _ALU_lane_latency, int _num_ALU_lanes) {
    state = Status::Idle;
    maa = _maa;
    ALU_lane_latency = _ALU_lane_latency;
    num_ALU_lanes = _num_ALU_lanes;
    my_instruction = nullptr;
}
void ALUUnit::executeInstruction() {
    switch (state) {
    case Status::Idle: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAALU, "%s: idling %s!\n", __func__, my_instruction->print());
        state = Status::Decode;
        [[fallthrough]];
    }
    case Status::Decode: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAALU, "%s: decoding %s!\n", __func__, my_instruction->print());

        // Decoding the instruction
        my_dst_tile = my_instruction->dst1SpdID;
        my_cond_tile = my_instruction->condSpdID;
        my_src1_tile = my_instruction->src1SpdID;
        my_src2_tile = my_instruction->src2SpdID;
        my_max = maa->spd->getSize(my_src1_tile);
        panic_if(my_src2_tile != -1 && my_max != maa->spd->getSize(my_src2_tile),
                 "%s: src1 size(%d) != src2 size(%d)!\n",
                 __func__, my_max, maa->spd->getSize(my_src2_tile));
        panic_if(my_cond_tile != -1 && my_max != maa->spd->getSize(my_cond_tile),
                 "%s: src1 size(%d) != cond size(%d)!\n",
                 __func__, my_max, maa->spd->getSize(my_cond_tile));
        if (my_instruction->datatype == Instruction::DataType::INT32_TYPE) {
            my_src2_reg_int32 = maa->rf->getData<int32_t>(my_instruction->src1RegID);
        } else if (my_instruction->datatype == Instruction::DataType::FLOAT32_TYPE) {
            my_src2_reg_float32 = maa->rf->getData<float>(my_instruction->src1RegID);
        } else {
            assert(false);
        }

        // Setting the state of the instruction and ALU unit
        DPRINTF(MAAALU, "%s: state set to work for request %s!\n", __func__, my_instruction->print());
        my_instruction->state = Instruction::Status::Service;
        state = Status::Work;
        [[fallthrough]];
    }
    case Status::Work: {
        assert(my_instruction != nullptr);
        DPRINTF(MAAALU, "%s: working %s!\n", __func__, my_instruction->print());
        int num_spd_read_accesses = 0;
        int num_spd_write_accesses = 0;
        for (int i = 0; i < my_max; i++) {
            if (my_cond_tile != -1) {
                num_spd_read_accesses++;
            }
            if (my_cond_tile == -1 || maa->spd->getData<uint32_t>(my_cond_tile, i) != 0) {
                switch (my_instruction->datatype) {
                case Instruction::DataType::INT32_TYPE: {
                    int32_t src1 = maa->spd->getData<int32_t>(my_src1_tile, i);
                    num_spd_read_accesses++;
                    int32_t src2;
                    if (my_instruction->opcode == Instruction::OpcodeType::ALU_SCALAR) {
                        src2 = my_src2_reg_int32;
                    } else {
                        src2 = maa->spd->getData<int32_t>(my_src2_tile, i);
                        num_spd_read_accesses++;
                    }
                    int32_t result_signed;
                    uint32_t result_unsigned;
                    switch (my_instruction->optype) {
                    case Instruction::OPType::ADD_OP:
                        result_signed = src1 + src2;
                        break;
                    case Instruction::OPType::SUB_OP:
                        result_signed = src1 - src2;
                        break;
                    case Instruction::OPType::MUL_OP:
                        result_signed = src1 * src2;
                        break;
                    case Instruction::OPType::DIV_OP:
                        result_signed = src1 / src2;
                        break;
                    case Instruction::OPType::MIN_OP:
                        result_signed = std::min(src1, src2);
                        break;
                    case Instruction::OPType::MAX_OP:
                        result_signed = std::max(src1, src2);
                        break;
                    case Instruction::OPType::GT_OP:
                        result_unsigned = src1 > src2 ? 1 : 0;
                        break;
                    case Instruction::OPType::GTE_OP:
                        result_unsigned = src1 >= src2 ? 1 : 0;
                        break;
                    case Instruction::OPType::LT_OP:
                        result_unsigned = src1 < src2 ? 1 : 0;
                        break;
                    case Instruction::OPType::LTE_OP:
                        result_unsigned = src1 <= src2 ? 1 : 0;
                        break;
                    case Instruction::OPType::EQ_OP:
                        result_unsigned = src1 == src2 ? 1 : 0;
                        break;
                    default:
                        assert(false);
                    }
                    num_spd_write_accesses++;
                    if (my_instruction->optype == Instruction::OPType::GT_OP ||
                        my_instruction->optype == Instruction::OPType::GTE_OP ||
                        my_instruction->optype == Instruction::OPType::LT_OP ||
                        my_instruction->optype == Instruction::OPType::LTE_OP ||
                        my_instruction->optype == Instruction::OPType::EQ_OP) {
                        maa->spd->setData<uint32_t>(my_dst_tile, i, result_unsigned);
                    } else {
                        maa->spd->setData<int32_t>(my_dst_tile, i, result_signed);
                    }
                    break;
                }
                case Instruction::DataType::FLOAT32_TYPE: {
                    float src1 = maa->spd->getData<float>(my_src1_tile, i);
                    num_spd_read_accesses++;
                    float src2;
                    if (my_instruction->opcode == Instruction::OpcodeType::ALU_SCALAR) {
                        src2 = my_src2_reg_float32;
                    } else {
                        src2 = maa->spd->getData<float>(my_src2_tile, i);
                        num_spd_read_accesses++;
                    }
                    float result_float;
                    uint32_t result_unsigned;
                    switch (my_instruction->optype) {
                    case Instruction::OPType::ADD_OP:
                        result_float = src1 + src2;
                        break;
                    case Instruction::OPType::SUB_OP:
                        result_float = src1 - src2;
                        break;
                    case Instruction::OPType::MUL_OP:
                        result_float = src1 * src2;
                        break;
                    case Instruction::OPType::DIV_OP:
                        result_float = src1 / src2;
                        break;
                    case Instruction::OPType::MIN_OP:
                        result_float = std::min(src1, src2);
                        break;
                    case Instruction::OPType::MAX_OP:
                        result_float = std::max(src1, src2);
                        break;
                    case Instruction::OPType::GT_OP:
                        result_unsigned = src1 > src2 ? 1 : 0;
                        break;
                    case Instruction::OPType::GTE_OP:
                        result_unsigned = src1 >= src2 ? 1 : 0;
                        break;
                    case Instruction::OPType::LT_OP:
                        result_unsigned = src1 < src2 ? 1 : 0;
                        break;
                    case Instruction::OPType::LTE_OP:
                        result_unsigned = src1 <= src2 ? 1 : 0;
                        break;
                    case Instruction::OPType::EQ_OP:
                        result_unsigned = src1 == src2 ? 1 : 0;
                        break;
                    default:
                        assert(false);
                    }
                    num_spd_write_accesses++;
                    if (my_instruction->optype == Instruction::OPType::GT_OP ||
                        my_instruction->optype == Instruction::OPType::GTE_OP ||
                        my_instruction->optype == Instruction::OPType::LT_OP ||
                        my_instruction->optype == Instruction::OPType::LTE_OP ||
                        my_instruction->optype == Instruction::OPType::EQ_OP) {
                        maa->spd->setData<uint32_t>(my_dst_tile, i, result_unsigned);
                    } else {
                        maa->spd->setData<float>(my_dst_tile, i, result_float);
                    }
                    break;
                }
                default:
                    assert(false);
                }
            }
        }
        Cycles get_data_latency = maa->spd->getDataLatency(num_spd_read_accesses);
        Cycles set_data_latency = maa->spd->setDataLatency(num_spd_write_accesses);
        int num_ALU_iterations = my_max / num_ALU_lanes;
        if (my_max % num_ALU_lanes != 0) {
            num_ALU_iterations++;
        }
        Cycles ALU_latency = Cycles(num_ALU_iterations * ALU_lane_latency);
        Cycles final_latency = std::max(get_data_latency, set_data_latency);
        final_latency = std::max(final_latency, ALU_latency);
        DPRINTF(MAAALU, "%s: setting state to finish for request %s in %d cycles!\n", __func__, my_instruction->print(), final_latency);
        state = Status::Finish;
        scheduleExecuteInstructionEvent(final_latency);
        break;
    }
    case Status::Finish: {
        DPRINTF(MAAALU, "%s: finishing %s!\n", __func__, my_instruction->print());
        my_instruction->state = Instruction::Status::Finish;
        state = Status::Idle;
        maa->spd->setReady(my_dst_tile);
        maa->spd->setSize(my_dst_tile, my_max);
        maa->finishInstruction(my_instruction, my_dst_tile);
        my_instruction = nullptr;
        break;
    }
    default:
        assert(false);
    }
}
void ALUUnit::setInstruction(Instruction *_instruction) {
    assert(my_instruction == nullptr);
    my_instruction = _instruction;
}
void ALUUnit::scheduleExecuteInstructionEvent(int latency) {
    DPRINTF(MAAALU, "%s: scheduling execute for the ALU Unit in the next %d cycles!\n", __func__, latency);
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