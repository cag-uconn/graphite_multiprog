#pragma once

#include <cassert>
#include <iostream>
using std::cout;
using std::endl;

#include "heap_allocator.h"
#include "config.h"
#include "log.h"

typedef int heap_id_t;

#define TRANSPORT_HEAP_ID  ((int) Config::getSingleton()->getTotalTiles())
#define NUM_HEAPS          ((int) Config::getSingleton()->getTotalTiles() + 1)

template <typename T>
class ScalableAllocator
{
public:
   static void initialize();
   static void printSummary();

   static void* operator new(std::size_t sz, heap_id_t heap_id);
   static void* operator new[] (std::size_t sz, heap_id_t heap_id)
   { return operator new(sz, heap_id); }

   static void operator delete(void* ptr);
   static void operator delete[] (void* ptr)
   { operator delete(ptr); }

private:
   static HeapAllocator* _heap_allocator_vec;
   static bool _initialized;
   static Lock _initialize_lock;
};

template <typename T>
HeapAllocator* ScalableAllocator<T>::_heap_allocator_vec;
template <typename T>
bool ScalableAllocator<T>::_initialized = false;
template <typename T>
Lock ScalableAllocator<T>::_initialize_lock;

template <typename T>
void ScalableAllocator<T>::initialize()
{
   ScopedLock sl(_initialize_lock);
   if (!_initialized)
   {
      _heap_allocator_vec = new HeapAllocator[NUM_HEAPS];
      _initialized = true;
   }
}

template <typename T>
void* ScalableAllocator<T>::operator new(size_t sz, heap_id_t heap_id)
{
   LOG_PRINT("new: sz(%u), heap_id(%i)", sz, heap_id);
   assert(heap_id < NUM_HEAPS);
   
   if (!_initialized)
      initialize();

   size_t metadata_sz = sizeof(heap_id_t) + sizeof(size_t);
   
   HeapAllocator& heap_allocator = _heap_allocator_vec[heap_id];
   char* mem = heap_allocator.allocate(sz + metadata_sz);
   LOG_PRINT("new: mem(%p)", mem);
   
   // initialize the memory with heap_id && sz
   *reinterpret_cast<heap_id_t*> (mem) = heap_id;
   *reinterpret_cast<size_t*> (mem + sizeof(heap_id_t)) = sz;
   LOG_PRINT("new: ptr(%p)", mem + metadata_sz);
   return (void*) (mem + metadata_sz);
}

template <typename T>
void ScalableAllocator<T>::operator delete(void* ptr)
{
   LOG_PRINT("delete: ptr(%p)", ptr);
   assert(_initialized);

   size_t metadata_sz = sizeof(heap_id_t) + sizeof(size_t);
   char* mem = (char*) ptr - metadata_sz;

   LOG_PRINT("delete: mem(%p)", mem);
   heap_id_t heap_id = *reinterpret_cast<heap_id_t*> (mem);
   size_t sz = *reinterpret_cast<size_t*> (mem + sizeof(heap_id_t));

   LOG_PRINT("delete: sz(%u), heap_id(%i)", sz, heap_id);
   assert(heap_id < NUM_HEAPS);

   HeapAllocator& heap_allocator = _heap_allocator_vec[heap_id];
   heap_allocator.free(sz + metadata_sz, mem);
}

template <typename T>
void ScalableAllocator<T>::printSummary()
{
   cout << "Scalable Allocator Summary: " << endl;
   HeapAllocator::Summary final_summary;
   for (int i = 0; i < NUM_HEAPS; i++)
   {
      HeapAllocator& heap_allocator = _heap_allocator_vec[i];
      HeapAllocator::Summary heap_summary;
      heap_allocator.gatherSummary(heap_summary);
      for (HeapAllocator::Summary::iterator it = heap_summary.begin();
            it != heap_summary.end(); it++)
      {
         size_t sz = (*it).first;
         FSBAllocator::Summary& fsb_summary = (*it).second;
         final_summary[sz].first += fsb_summary.first;
         final_summary[sz].second += fsb_summary.second;
      }
   }
   
   size_t metadata_sz = sizeof(heap_id_t) + sizeof(size_t);
   for (HeapAllocator::Summary::iterator it = final_summary.begin();
         it != final_summary.end(); it++)
   {
      size_t sz = (*it).first;
      FSBAllocator::Summary fsb_summary = (*it).second;
      cout << "Size: " << sz - metadata_sz
           << ", Num-Blocks: " << fsb_summary.first
           << ", Num-Allocations: " << fsb_summary.second << endl;
   }
}
