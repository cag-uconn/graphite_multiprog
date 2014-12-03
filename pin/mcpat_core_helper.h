#pragma once

#include "mcpat_info.h"
#include "instruction_modeling.h"

void fillMcPATMicroOpList(McPATInfo::MicroOpList& micro_op_list,
                          InstructionType type, UInt32 num_read_memory_operands, UInt32 num_write_memory_operands);
void fillMcPATRegisterFileAccessCounters(McPATInfo::RegisterFile& register_file,
                                         const RegisterOperandList& read_register_operands,
                                         const RegisterOperandList& write_register_operands);
void fillMcPATExecutionUnitList(McPATInfo::ExecutionUnitList& execution_unit_list,
                                InstructionType instruction_type);

// Utils
bool isIntegerReg(const RegisterOperand& reg);
bool isFloatingPointReg(const RegisterOperand& reg_id);
bool isXMMReg(const RegisterOperand& reg_id);
