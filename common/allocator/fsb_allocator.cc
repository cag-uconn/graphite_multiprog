#include "fsb_allocator.h"
#include "scalable_allocator.h"

FSBAllocator::FSBAllocator(size_t sz)
   : _sz(sz)
   , _num_blocks(0)
   , _num_allocations(0)
{}

FSBAllocator::~FSBAllocator()
{}

char*
FSBAllocator::allocate()
{
   _num_allocations ++;
   
   if (_free_list.empty())
   {
      // Increase memory to be allocated (additive or multiplicative)
      // uint32_t added_blocks = (_num_blocks > 0) ? _num_blocks : 1;
      uint32_t added_blocks = 1;
      
      // allocate new memory
      char* new_mem = (char*) ::operator new(added_blocks * _sz);
      // Add to free list
      for (uint32_t i = 0; i < added_blocks; i++)
         _free_list.push(new_mem + i * _sz);

      // Update the total number of blocks allocated
      _num_blocks += added_blocks;
   }

   // Return the memory
   char* mem = _free_list.top();
   _free_list.pop();
   return mem;
}

void
FSBAllocator::free(char* mem)
{
   _free_list.push(mem);
}

void
FSBAllocator::gatherSummary(Summary& summary)
{
   summary = make_pair(_num_blocks, _num_allocations);
}
