#ifndef __MEM_MAA_ALU_HH__
#define __MEM_MAA_ALU_HH__

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>

#include "mem/MAA/IF.hh"

namespace gem5 {

class MAA;

class ALUUnit {
public:
    enum class Status : uint8_t {
        Idle = 0,
        Decode = 1,
        Work = 2,
        max
    };

protected:
    std::string status_names[4] = {
        "Idle",
        "Decode",
        "Work",
        "max"};
    Status state;
    MAA *maa;

public:
    ALUUnit();

    void allocate(MAA *_maa) {
        state = Status::Idle;
        maa = _maa;
    }
    Status getState() const { return state; }

    void setInstruction(Instruction *_instruction);

    void scheduleExecuteInstructionEvent(int latency = 0);

protected:
    Instruction *my_instruction;
    int my_dst_tile, my_cond_tile, my_src1_tile, my_src2_tile;
    int my_src2_reg_int32;
    float my_src2_reg_float32;
    int my_max;
    Instruction::OPType my_optype;
    Instruction::OpcodeType my_opcode;
    Instruction::DataType my_datatype;

    void executeInstruction();
    EventFunctionWrapper executeInstructionEvent;
};
} // namespace gem5

#endif // __MEM_MAA_ALU_HH__