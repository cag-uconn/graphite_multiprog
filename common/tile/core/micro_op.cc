#include <string.h>

#include "micro_op.h"
#include "log.h"

string
MicroOp::getTypeStr() const
{
   switch (type)
   {
   case MicroOp::GENERAL:
      return "GENERAL";
   case MicroOp::BRANCH:
      return "BRANCH";
   case MicroOp::LOAD:
      return "LOAD";
   case MicroOp::STORE:
      return "STORE";
   case MicroOp::STORE_ADDR:
      return "STORE_ADDR";
   case MicroOp::FENCE:
      return "FENCE";
   default:
      LOG_PRINT_ERROR("Unrecognized type: %u", type);
      return "";
   }
}

void
MicroOp::clear()
{
    memset(this, 0, sizeof(MicroOp));  // NOTE: This may break if DynUop becomes non-POD
}
