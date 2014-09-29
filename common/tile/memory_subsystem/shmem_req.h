#pragma once

#include "scalable_allocator.h"

class ShmemReq : public ScalableAllocator<ShmemReq>
{
public:
   ShmemReq() {}
   virtual ~ShmemReq() {}
};
