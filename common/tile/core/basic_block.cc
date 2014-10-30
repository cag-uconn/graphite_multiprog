#include "basic_block.h"


BasicBlock::BasicBlock(uint32_t nInstrs, uintptr_t address, uint32_t size,
                       uint32_t nUops, MicroOp* uopArray)
   : _nInstrs(nInstrs)
   , _address(address)
   , _size(size)
   , _nUops(nUops)
   , _uopArray(uopArray)
{}
