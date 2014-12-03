#pragma once

#include <vector>
#include <stdint.h>
using std::vector;

#include <pin.H>

enum InstructionType
{
   INST_GENERIC,
   INST_MOV,
   INST_IALU,
   INST_IMUL,
   INST_IDIV,
   INST_FALU,
   INST_FMUL,
   INST_FDIV,
   INST_XMM_SS,
   INST_XMM_SD,
   INST_XMM_PS,
   INST_XMM_PD,
   INST_BRANCH,
   INST_FENCE
};

typedef uint32_t RegisterOperand;
typedef vector<RegisterOperand> RegisterOperandList;
typedef vector<uint64_t> ImmediateOperandList;

void addInstructionModeling(INS ins);
