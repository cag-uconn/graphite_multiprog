#pragma once

#include <cstring>
#include <cstdint>
#include <stack>
using std::stack;
using std::pair;
using std::make_pair;

class FSBAllocator
{
public:
   typedef pair<uint32_t, uint64_t> Summary;

   FSBAllocator(size_t sz);
   ~FSBAllocator();

   char* allocate();
   void free(char* mem);

   void gatherSummary(Summary& summary);

private: 
   typedef stack<char*> FreeList;
   FreeList _free_list;
   size_t _sz;
   uint32_t _num_blocks;
   uint64_t _num_allocations;
};
