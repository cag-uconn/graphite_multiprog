#include "fsb_allocator.h"
#include "scalable_allocator.h"

FSBAllocator::FSBAllocator(size_t sz, uint32_t initial_blocks)
   : _free_block(NULL)
   , _metadata_sz(sizeof(char*))
   , _sz(sz + _metadata_sz)
   , _num_blocks(0)
   , _num_allocations(0)
{
   if (initial_blocks > 0)
      populateFreeList(initial_blocks);
}

FSBAllocator::~FSBAllocator()
{}

char*
FSBAllocator::allocate()
{
   _num_allocations ++;
   if (_free_block == NULL)
   {
      // Increase memory to be allocated (additive or multiplicative)
      uint32_t added_blocks = (_num_blocks > 0) ? _num_blocks : 1;
      // uint32_t added_blocks = 1;
      populateFreeList(added_blocks);
   }

   // Return the memory
   char* mem = _free_block;
   // Point to next block
   _free_block = *(reinterpret_cast<char**>(_free_block));
   return (mem + _metadata_sz);
}

void
FSBAllocator::free(char* ptr)
{
   char* mem = ptr - _metadata_sz;
   *(reinterpret_cast<char**>(mem)) = _free_block;
   _free_block = mem;
}

void
FSBAllocator::populateFreeList(uint32_t added_blocks)
{
   assert(added_blocks > 0);
   // allocate new memory
   char* new_mem = (char*) ::operator new(added_blocks * _sz);
   // Initialize free block
   _free_block = new_mem;
   // Add to free list
   for (uint32_t i = 0; i < added_blocks-1; i++)
   {
      *(reinterpret_cast<char**>(new_mem)) = new_mem + _sz;
      new_mem += _sz;
   }
   *(reinterpret_cast<char**>(new_mem)) = NULL;

   // Update the total number of blocks allocated
   _num_blocks += added_blocks;
}

void
FSBAllocator::gatherSummary(Summary& summary)
{
   summary = make_pair(_num_blocks, _num_allocations);
}
