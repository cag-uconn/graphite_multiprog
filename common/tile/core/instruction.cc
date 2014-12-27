#include "instruction.h"
#include "simulator.h"
#include "tile_manager.h"
#include "tile.h"
#include "core.h"
#include "core_model.h"
#include "branch_predictor.h"

// Instruction

Instruction::Instruction(uintptr_t address, uint32_t size,
                         uint32_t nUops, MicroOp* uopArray)
   : _address(address)
   , _size(size)
   , _nUops(nUops)
   , _uopArray(uopArray)
   , _mcpatInfo(NULL)
{}

// DynamicInstruction
DynamicInstruction::DynamicInstruction(Type type, const Time& cost)
   : _type(type), _cost(cost)
{}

string
DynamicInstruction::getTypeStr() const
{
   switch (_type)
   {
   case NETRECV:
      return "NETRECV";
   case SYNC:
      return "SYNC";
   case SPAWN:
      return "SPAWN";
   default:
      LOG_PRINT_ERROR("Unrecognized type: %u", _type);
      return "";
   }
}
