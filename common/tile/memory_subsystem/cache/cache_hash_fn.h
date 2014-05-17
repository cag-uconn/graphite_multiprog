#pragma once

#include "utils.h"
#include "constants.h"
#include "hash_fn.h"

class CacheHashFn : public HashFn
{
public:
   CacheHashFn(uint32_t cache_size, uint32_t associativity, uint32_t cache_line_size)
      : HashFn()
      , _num_sets(cache_size * k_KILO / (cache_line_size * associativity))
      , _log_cache_line_size(floorLog2(cache_line_size))
   {}
   ~CacheHashFn()
   {}

   uint32_t compute(uintptr_t address)
   { return ((uint32_t)(address >> _log_cache_line_size)) & (_num_sets-1); }

protected:
   uint32_t _num_sets;
   uint32_t _log_cache_line_size;
};
