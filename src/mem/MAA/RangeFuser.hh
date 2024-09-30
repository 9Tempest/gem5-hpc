#ifndef __MEM_MAA_RANGEFUSER_HH__
#define __MEM_MAA_RANGEFUSER_HH__

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include "sim/system.hh"

namespace gem5 {

class MAA;
class Instruction;

class RangeFuserUnit {
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
    RangeFuserUnit();

    void allocate(unsigned int _num_tile_elements, MAA *_maa);

    Status getState() const { return state; }

    void setInstruction(Instruction *_instruction);

    void scheduleExecuteInstructionEvent(int latency = 0);

protected:
    Instruction *my_instruction;
    int my_dst_i_tile, my_dst_j_tile, my_cond_tile, my_min_tile, my_max_tile;
    int my_last_i, my_last_j, my_stride;
    int my_max_i, my_idx_j;
    unsigned int num_tile_elements;

    void executeInstruction();
    EventFunctionWrapper executeInstructionEvent;
};
} // namespace gem5

#endif // __MEM_MAA_RANGEFUSER_HH__