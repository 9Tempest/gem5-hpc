#ifndef __MEM_MAA_IF_HH__
#define __MEM_MAA_IF_HH__

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>

#include "base/types.hh"
#include "sim/system.hh"
#include "arch/generic/mmu.hh"

namespace gem5 {
enum class FuncUnitType : uint8_t {
    STREAM = 0,
    INDIRECT = 1,
    INVALIDATOR = 2,
    ALU = 3,
    RANGE = 4,
    MAX
};
const std::string func_unit_names[6] = {
    "STREAM",
    "INDIRECT",
    "INVALIDATOR",
    "ALU",
    "RANGE",
    "MAX"};
class Instruction {
public:
    enum class OpcodeType : uint8_t {
        STREAM_LD = 0,
        STREAM_ST = 1,
        INDIR_LD = 2,
        INDIR_ST = 3,
        INDIR_RMW = 4,
        RANGE_LOOP = 5,
        ALU_SCALAR = 6,
        ALU_VECTOR = 7,
        ALU_REDUCE = 8,
        MAX
    };
    std::string opcode_names[9] = {
        "STREAM_LD",
        "STREAM_ST",
        "INDIR_LD",
        "INDIR_ST",
        "INDIR_RMW",
        "RANGE_LOOP",
        "ALU_SCALAR",
        "ALU_VECTOR",
        "ALU_REDUCE"};
    enum class OPType : uint8_t {
        ADD_OP = 0,
        SUB_OP = 1,
        MUL_OP = 2,
        DIV_OP = 3,
        MIN_OP = 4,
        MAX_OP = 5,
        AND_OP = 6,
        OR_OP = 7,
        XOR_OP = 8,
        SHL_OP = 9,
        SHR_OP = 10,
        GT_OP = 11,
        GTE_OP = 12,
        LT_OP = 13,
        LTE_OP = 14,
        EQ_OP = 15,
        MAX
    };
    std::string optype_names[11] = {
        "ADD",
        "SUB",
        "MUL",
        "DIV",
        "MIN",
        "MAX",
        "GT",
        "GTE",
        "LT",
        "LTE",
        "EQ"};
    enum class DataType : uint8_t {
        UINT32_TYPE = 0,
        INT32_TYPE = 1,
        FLOAT32_TYPE = 2,
        UINT64_TYPE = 3,
        INT64_TYPE = 4,
        FLOAT64_TYPE = 5,
        MAX
    };
    std::string datatype_names[6] = {
        "UINT32",
        "INT32",
        "FLOAT32",
        "UINT64",
        "INT64",
        "FLOAT64"};
    enum class Status : uint8_t {
        Idle = 0,
        Service = 1,
        Finish = 2,
        MAX
    };
    std::string status_names[4] = {
        "Idle",
        "Service",
        "Finish",
        "MAX"};
    enum class TileStatus : uint8_t {
        WaitForInvalidation = 0,
        Invalidating = 1,
        WaitForService = 2,
        Service = 3,
        Finished = 4,
        MAX
    };
    std::string tile_status_names[6] = {
        "WFI",
        "INV",
        "WFS",
        "SRV",
        "FNS",
        "MAX"};
    Addr baseAddr;
    int16_t src1RegID, src2RegID, src3RegID, dst1RegID, dst2RegID;
    int16_t src1SpdID, src2SpdID;
    TileStatus src1Status, src2Status;
    int16_t dst1SpdID, dst2SpdID;
    TileStatus dst1Status, dst2Status;
    int16_t condSpdID;
    TileStatus condStatus;
    // {STREAM_LD, INDIR_LD, INDIR_ST, INDIR_RMW, RANGE_LOOP, CONDITION}
    OpcodeType opcode;
    // {ADD, SUB, MUL, DIV, MIN, MAX, GT, GTE, LT, LTE, EQ}
    OPType optype;
    // {Int, Float}
    DataType datatype;
    // {Idle, Translation, Fill, Request, Response}
    Status state;
    // {ALU, STREAM, INDIRECT}
    FuncUnitType funcUniType;
    int funcUniID;
    ContextID CID;
    Addr PC;
    int if_id;
    Instruction();
    std::string print() const;
    int getWordSize(int tile_id);

protected:
    int WordSize();
};

class IF {
protected:
    Instruction *instructions;
    unsigned int num_instructions;
    bool *valids;
    Instruction::TileStatus getTileStatus(int tile_id, uint8_t tile_status);

public:
    IF(unsigned int _num_instructions) : num_instructions(_num_instructions) {
        instructions = new Instruction[num_instructions];
        valids = new bool[num_instructions];
        for (int i = 0; i < num_instructions; i++) {
            valids[i] = false;
        }
    }
    ~IF() {
        assert(instructions != nullptr);
        assert(valids != nullptr);
        delete[] instructions;
        delete[] valids;
    }
    bool pushInstruction(Instruction _instruction);
    Instruction *getReady(FuncUnitType funcUniType);
    void finishInstructionCompute(Instruction *instruction);
    void finishInstructionInvalidate(Instruction *instruction, int tile_id, uint8_t tile_status);
    void issueInstructionCompute(Instruction *instruction);
    void issueInstructionInvalidate(Instruction *instruction, int tile_id);
};

class AddressRangeType {
protected:
    Addr addr;
    Addr base;
    Addr offset;
    uint8_t rangeID;
    bool valid;

public:
    static const char *const address_range_names[7];
    enum class Type : uint8_t {
        SPD_DATA_CACHEABLE_RANGE = 0,
        SPD_DATA_NONCACHEABLE_RANGE = 1,
        SPD_SIZE_RANGE = 2,
        SPD_READY_RANGE = 3,
        SCALAR_RANGE = 4,
        INSTRUCTION_RANGE = 5,
        MAX = 6
    };
    AddressRangeType(Addr _addr, AddrRangeList addrRanges);
    std::string print() const;
    Type getType() const { return static_cast<Type>(rangeID); }
    Addr getOffset() const { return offset; }
    bool isValid() const { return valid; }
};
} // namespace gem5

#endif // __MEM_MAA_IF_HH__