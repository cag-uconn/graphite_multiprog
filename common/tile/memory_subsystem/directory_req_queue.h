#pragma once

#include <map>
#include <stack>
using std::map;
using std::stack;

#include "fixed_types.h"
#include "shmem_req.h"

class DirectoryReqQueue
{
public:
   DirectoryReqQueue();
   ~DirectoryReqQueue();

   void push(IntPtr address, ShmemReq* shmem_req);
   void pop(IntPtr address);
   ShmemReq* front(IntPtr address) const;
   size_t size(IntPtr address) const;
   bool empty(IntPtr address) const;

private:
   struct Queue
   {
      struct Node
      {
         ShmemReq* _req;
         Node* _next;
      };
      Queue(Node* head, Node* tail): _head(head), _tail(tail), _size(1) {}
      Node* _head;
      Node* _tail;
      size_t _size;
   };

   typedef map<IntPtr, Queue> AddressMap;
   typedef stack<Queue::Node*> FreeList;

   AddressMap _address_map;
   FreeList _free_list;
};
