#pragma once

#include <map>
using std::map;

#include "lock.h"
#include "fsb_allocator.h"

class HeapAllocator
{
public:
   typedef map<size_t, FSBAllocator::Summary> Summary;

   HeapAllocator() {}
   ~HeapAllocator() {}

   char* allocate(size_t sz);
   void free(size_t sz, char* mem);

   void gatherSummary(Summary& summary);

private:
   typedef map<size_t, FSBAllocator> FSBAllocatorMap;
   FSBAllocatorMap _fsb_allocator_map;
   Lock _lock;
};
