#ifndef __MEM_MAA_ALU_HH__
#define __MEM_MAA_ALU_HH__

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include "sim/system.hh"

namespace gem5 {

class MAA;
class Instruction;

class ALUUnit {
public:
    enum class Status : uint8_t {
        Idle = 0,
        Decode = 1,
        Work = 2,
        Finish = 3,
        max
    };

protected:
    std::string status_names[5] = {
        "Idle",
        "Decode",
        "Work",
        "Finish",
        "max"};
    Status state;
    MAA *maa;

public:
    ALUUnit();

    void allocate(MAA *_maa, int _my_alu_id, Cycles _ALU_lane_latency, int _num_ALU_lanes);

    Status getState() const { return state; }

    void setInstruction(Instruction *_instruction);

    void scheduleExecuteInstructionEvent(int latency = 0);

protected:
    Instruction *my_instruction;
    int my_alu_id;
    int my_dst_tile, my_cond_tile, my_src1_tile, my_src2_tile;
    int my_max;
    int my_word_size;
    Cycles ALU_lane_latency;
    int num_ALU_lanes;

    void executeInstruction();
    EventFunctionWrapper executeInstructionEvent;
};
} // namespace gem5

#endif // __MEM_MAA_ALU_HH__