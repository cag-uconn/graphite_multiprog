#pragma once

#include <cstdint>

class HashFn
{
public:
   HashFn() {}
   virtual ~HashFn() {}
   virtual uint32_t compute(uintptr_t address) = 0;
};
