#include "mem/MAA/IF.hh"
#include "mem/MAA/MAA.hh"
#include "debug/MAA.hh"
#include <cassert>

#ifndef TRACING_ON
#define TRACING_ON 1
#endif

namespace gem5 {
Instruction::Instruction() : baseAddr(0),
                             src1RegID(-1),
                             src2RegID(-1),
                             src3RegID(-1),
                             dst1RegID(-1),
                             dst2RegID(-1),
                             src1SpdID(-1),
                             src2SpdID(-1),
                             src1Ready(false),
                             src2Ready(false),
                             dst1SpdID(-1),
                             dst2SpdID(-1),
                             condSpdID(-1),
                             opcode(OpcodeType::MAX),
                             optype(OPType::MAX),
                             datatype(DataType::MAX),
                             state(Status::Idle),
                             funcUnit(FuncUnitType::MAX),
                             CID(-1),
                             PC(0),
                             if_id(-1) {}
std::string Instruction::print() const {
    std::ostringstream str;
    ccprintf(str, "INSTR[%s%s%s%s%s%s%s%s%s%s%s%s%s%s]",
             "opcode(" + opcode_names[(int)opcode] + ")",
             optype == OPType::MAX ? "" : " optype(" + optype_names[(int)optype] + ")",
             " datatype(" + datatype_names[(int)datatype] + ")",
             " state(" + status_names[(int)state] + ")",
             src1SpdID == -1 ? "" : " srcSPD1(" + std::to_string(src1SpdID) + ")",
             src2SpdID == -1 ? "" : " srcSPD2(" + std::to_string(src2SpdID) + ")",
             src1RegID == -1 ? "" : " srcREG1(" + std::to_string(src1RegID) + ")",
             src2RegID == -1 ? "" : " srcREG2(" + std::to_string(src2RegID) + ")",
             src3RegID == -1 ? "" : " srcREG3(" + std::to_string(src3RegID) + ")",
             dst1SpdID == -1 ? "" : " dstSPD1(" + std::to_string(dst1SpdID) + ")",
             dst2SpdID == -1 ? "" : " dstSPD2(" + std::to_string(dst2SpdID) + ")",
             dst1RegID == -1 ? "" : " dstREG1(" + std::to_string(dst1RegID) + ")",
             dst2RegID == -1 ? "" : " dstREG2(" + std::to_string(dst2RegID) + ")",
             condSpdID == -1 ? "" : " condSPD(" + std::to_string(condSpdID) + ")");
    return str.str();
}
bool IF::pushInstruction(Instruction _instruction) {
    switch (_instruction.opcode) {
    case Instruction::OpcodeType::STREAM_LD: {
        _instruction.funcUnit = FuncUnitType::STREAM;
        break;
    }
    case Instruction::OpcodeType::INDIR_LD:
    case Instruction::OpcodeType::INDIR_ST:
    case Instruction::OpcodeType::INDIR_RMW: {
        _instruction.funcUnit = FuncUnitType::INDIRECT;
        break;
    }
    case Instruction::OpcodeType::RANGE_LOOP: {
        _instruction.funcUnit = FuncUnitType::RANGE;
        break;
    }
    case Instruction::OpcodeType::ALU_SCALAR:
    case Instruction::OpcodeType::ALU_VECTOR: {
        _instruction.funcUnit = FuncUnitType::ALU;
        break;
    }
    default: {
        assert(false);
    }
    }
    int free_instruction_slot = -1;
    for (int i = 0; i < num_instructions; i++) {
        if (valids[i] == false) {
            if (free_instruction_slot == -1) {
                free_instruction_slot = i;
            }
        } else {
            if (_instruction.dst1SpdID != -1) {
                if ((instructions[i].dst1SpdID != -1 && _instruction.dst1SpdID == instructions[i].dst1SpdID) ||
                    (instructions[i].dst2SpdID != -1 && _instruction.dst1SpdID == instructions[i].dst2SpdID) ||
                    (instructions[i].src1SpdID != -1 && _instruction.dst1SpdID == instructions[i].src1SpdID) ||
                    (instructions[i].src2SpdID != -1 && _instruction.dst1SpdID == instructions[i].src2SpdID)) {
                    return false;
                }
            }
            if (_instruction.dst2SpdID != -1) {
                if ((instructions[i].dst1SpdID != -1 && _instruction.dst2SpdID == instructions[i].dst1SpdID) ||
                    (instructions[i].dst2SpdID != -1 && _instruction.dst2SpdID == instructions[i].dst2SpdID) ||
                    (instructions[i].src1SpdID != -1 && _instruction.dst2SpdID == instructions[i].src1SpdID) ||
                    (instructions[i].src2SpdID != -1 && _instruction.dst2SpdID == instructions[i].src2SpdID)) {
                    return false;
                }
            }
        }
    }
    if (free_instruction_slot == -1) {
        return false;
    }
    assert(free_instruction_slot < num_instructions);
    instructions[free_instruction_slot] = _instruction;
    valids[free_instruction_slot] = true;
    instructions[free_instruction_slot].if_id = free_instruction_slot;
    return true;
}
Instruction *IF::getReady(FuncUnitType funcUnit) {
    if (funcUnit == FuncUnitType::INVALIDATOR) {
        for (int i = 0; i < num_instructions; i++) {
            if (valids[i] &&
                instructions[i].dst1Ready == false) {
                // instruction state does not matter anymore as we only have one invalidator
                return &instructions[i];
            }
        }
    } else {
        for (int i = 0; i < num_instructions; i++) {
            if (valids[i] &&
                instructions[i].src1Ready &&
                instructions[i].src2Ready &&
                instructions[i].dst1Ready &&
                instructions[i].state == Instruction::Status::Idle &&
                instructions[i].funcUnit == funcUnit) {
                instructions[i].state = Instruction::Status::Service;
                return &instructions[i];
            }
        }
    }
    return nullptr;
}
void IF::finishInstruction(Instruction *instruction,
                           int dst1SpdID,
                           int dst2SpdID) {
    instruction->state = Instruction::Status::Finish;
    valids[instruction->if_id] = false;
    if (dst1SpdID != -1) {
        for (int i = 0; i < num_instructions; i++) {
            if (valids[i]) {
                if (instructions[i].src1SpdID == dst1SpdID) {
                    instructions[i].src1Ready = true;
                }
                if (instructions[i].src2SpdID == dst1SpdID) {
                    instructions[i].src2Ready = true;
                }
            }
        }
    }
    if (dst2SpdID != -1) {
        for (int i = 0; i < num_instructions; i++) {
            if (valids[i]) {
                if (instructions[i].src1SpdID == dst2SpdID) {
                    instructions[i].src1Ready = true;
                }
                if (instructions[i].src2SpdID == dst2SpdID) {
                    instructions[i].src2Ready = true;
                }
            }
        }
    }
}
AddressRangeType::AddressRangeType(Addr _addr, AddrRangeList addrRanges) : addr(_addr) {
    bool range_found = false;
    rangeID = 0;
    for (const auto &r : addrRanges) {
        if (r.contains(addr)) {
            base = r.start();
            offset = addr - base;
            range_found = true;
            break;
        }
        rangeID++;
    }
    assert(range_found);
}
std::string AddressRangeType::print() const {
    std::ostringstream str;
    ccprintf(str, "%s: 0x%lx + 0x%lx", address_range_names[rangeID], base, offset);
    return str.str();
}
} // namespace gem5