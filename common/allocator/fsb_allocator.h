#pragma once

#include <cstring>
#include <cstdint>
#include <utility>
using std::pair;
using std::make_pair;

class FSBAllocator
{
public:
   typedef pair<uint32_t, uint64_t> Summary;

   FSBAllocator(size_t sz, uint32_t num_blocks = 0);
   ~FSBAllocator();

   char* allocate();
   void free(char* mem);

   void gatherSummary(Summary& summary);

private: 
   char* _free_block;
   size_t _metadata_sz;
   size_t _sz;
   uint32_t _num_blocks;
   uint64_t _num_allocations;

   void populateFreeList(uint32_t added_blocks);
};
