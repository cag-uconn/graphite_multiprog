#pragma once

#include <vector>
#include <stdint.h>
#include "micro_op.h"

class BasicBlock
{
public:
   BasicBlock(uint32_t nInstrs, uintptr_t address, uint32_t size,
              uint32_t nUops, MicroOp* uopArray);

   uint32_t getNumInstrs() const    { return _nInstrs;   }
   uintptr_t getAddress() const     { return _address;   }
   uint32_t getSize() const         { return _size;      }
   uint32_t getNumUops() const      { return _nUops;     }
   MicroOp* getUopArray() const     { return _uopArray;  }

private:
   uint32_t _nInstrs;
   uintptr_t _address;
   uint32_t _size;
   uint32_t _nUops;
   MicroOp* _uopArray;
#ifdef TARGET_PROFILING
   uint64_t _count;
   std::vector<uint32_t>* approxOpcodes;
#endif
};
