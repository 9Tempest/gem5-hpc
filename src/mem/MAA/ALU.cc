#include "mem/MAA/ALU.hh"
#include "base/trace.hh"
#include "mem/MAA/IF.hh"
#include "mem/MAA/MAA.hh"
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
}
void ALUUnit::allocate(MAA *_maa) {
    state = Status::Idle;
    maa = _maa;
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
        my_optype = my_instruction->optype;
        my_opcode = my_instruction->opcode;
        my_datatype = my_instruction->datatype;
        my_max = maa->spd->getSize(my_src1_tile);
        panic_if(my_src2_tile != -1 && my_max != maa->spd->getSize(my_src2_tile),
                 "%s: src1 size(%d) != src2 size(%d)!\n",
                 __func__, my_max, maa->spd->getSize(my_src2_tile));
        panic_if(my_cond_tile != -1 && my_max != maa->spd->getSize(my_cond_tile),
                 "%s: src1 size(%d) != cond size(%d)!\n",
                 __func__, my_max, maa->spd->getSize(my_cond_tile));
        if (my_datatype == Instruction::DataType::INT32_TYPE) {
            my_src2_reg_int32 = *((int32_t *)maa->rf->getDataPtr(my_instruction->src1RegID));
        } else if (my_datatype == Instruction::DataType::FLOAT32_TYPE) {
            my_src2_reg_float32 = *((float *)maa->rf->getDataPtr(my_instruction->src1RegID));
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
        for (int i = 0; i < my_max; i++) {
            if (my_cond_tile == -1 || maa->spd->getData(my_cond_tile, i) != 0) {
                switch (my_datatype) {
                case Instruction::DataType::INT32_TYPE: {
                    int32_t src1 = *((int32_t *)maa->spd->getDataPtr(my_src1_tile, i));
                    int32_t src2 = (my_opcode == Instruction::OpcodeType::ALU_SCALAR)
                                       ? my_src2_reg_int32
                                       : *((int32_t *)maa->spd->getDataPtr(my_src2_tile, i));
                    int32_t result_signed;
                    uint32_t result_unsigned;
                    switch (my_optype) {
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
                    if (my_optype == Instruction::OPType::GT_OP ||
                        my_optype == Instruction::OPType::GTE_OP ||
                        my_optype == Instruction::OPType::LT_OP ||
                        my_optype == Instruction::OPType::LTE_OP ||
                        my_optype == Instruction::OPType::EQ_OP) {
                        *((uint32_t *)maa->spd->getDataPtr(my_dst_tile, i)) = result_unsigned;
                    } else {
                        *((int32_t *)maa->spd->getDataPtr(my_dst_tile, i)) = result_signed;
                    }
                    break;
                }
                case Instruction::DataType::FLOAT32_TYPE: {
                    float src1 = *((float *)maa->spd->getDataPtr(my_src1_tile, i));
                    float src2 = (my_opcode == Instruction::OpcodeType::ALU_SCALAR)
                                     ? my_src2_reg_float32
                                     : *((float *)maa->spd->getDataPtr(my_src2_tile, i));
                    float result_float;
                    uint32_t result_unsigned;
                    switch (my_optype) {
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
                    if (my_optype == Instruction::OPType::GT_OP ||
                        my_optype == Instruction::OPType::GTE_OP ||
                        my_optype == Instruction::OPType::LT_OP ||
                        my_optype == Instruction::OPType::LTE_OP ||
                        my_optype == Instruction::OPType::EQ_OP) {
                        *((uint32_t *)maa->spd->getDataPtr(my_dst_tile, i)) = result_unsigned;
                    } else {
                        *((float *)maa->spd->getDataPtr(my_dst_tile, i)) = result_float;
                    }
                    break;
                }
                default:
                    assert(false);
                }
            }
        }
        DPRINTF(MAAALU, "%s: state set to finish for request %s!\n", __func__, my_instruction->print());
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