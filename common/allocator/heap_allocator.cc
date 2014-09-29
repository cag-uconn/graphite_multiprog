#include <cassert>
#include "heap_allocator.h"
#include "log.h"

char*
HeapAllocator::allocate(size_t sz)
{
   ScopedLock sl(_lock);
   FSBAllocatorMap::iterator it = _fsb_allocator_map.find(sz);
   if (it == _fsb_allocator_map.end())
   {
      _fsb_allocator_map.insert(make_pair(sz, FSBAllocator(sz)));
      it = _fsb_allocator_map.find(sz);
      assert (it != _fsb_allocator_map.end());
   }
   FSBAllocator& fsb_allocator = (*it).second;
   return fsb_allocator.allocate();
}

void
HeapAllocator::free(size_t sz, char* mem)
{
   ScopedLock sl(_lock);
   FSBAllocatorMap::iterator it = _fsb_allocator_map.find(sz);
   assert (it != _fsb_allocator_map.end());
   FSBAllocator& fsb_allocator = (*it).second;
   fsb_allocator.free(mem);
}

void
HeapAllocator::gatherSummary(Summary& summary)
{
   for (FSBAllocatorMap::iterator it = _fsb_allocator_map.begin();
         it != _fsb_allocator_map.end(); it++)
   {
      size_t sz = (*it).first;
      FSBAllocator& fsb_allocator = (*it).second;
      fsb_allocator.gatherSummary(summary[sz]);
   }
}
