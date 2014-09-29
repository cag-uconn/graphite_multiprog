#include "directory_req_queue.h"
#include "config.h"

DirectoryReqQueue::DirectoryReqQueue()
{
   _node_allocator = new FSBAllocator(sizeof(Queue::Node), Config::getSingleton()->getTotalTiles());
}

DirectoryReqQueue::~DirectoryReqQueue()
{
   delete _node_allocator;
}

void
DirectoryReqQueue::push(IntPtr address, ShmemReq* shmem_req)
{
   // Get node from free list
   Queue::Node* node = (Queue::Node*) _node_allocator->allocate();
   
   // Initialize node
   node->_req = shmem_req;
   node->_next = NULL;

   AddressMap::iterator it = _address_map.find(address);
   if (it == _address_map.end())
   {
      // Queue is empty
      _address_map.insert(make_pair(address, Queue(node, node)));
   }
   else // (it != _address_map.end())
   {
      // Queue has contents
      Queue& queue = (*it).second;
      queue._tail->_next = node;
      queue._tail = node;
      queue._size ++;
   }
}

void
DirectoryReqQueue::pop(IntPtr address)
{
   AddressMap::iterator it = _address_map.find(address);
   assert(it != _address_map.end());
   
   Queue& queue = (*it).second;
   Queue::Node* next_node = queue._head->_next;

   _node_allocator->free((char*) queue._head);

   queue._head = next_node;
   queue._size --;
   assert((queue._size == 0) == (queue._head == NULL));

   // Check if queue becomes empty
   if (queue._head == NULL)
      _address_map.erase(it);
}

ShmemReq*
DirectoryReqQueue::front(IntPtr address) const
{
   AddressMap::const_iterator it = _address_map.find(address);
   if (it != _address_map.end())
   {
      const Queue& queue = (*it).second;
      return queue._head->_req;
   }
   else
   {
      return NULL;
   }
}

size_t
DirectoryReqQueue::size(IntPtr address) const
{
   AddressMap::const_iterator it = _address_map.find(address);
   if (it != _address_map.end())
   {
      const Queue& queue = (*it).second;
      return queue._size;      
   }
   else
   {
      return 0;
   }
}

bool
DirectoryReqQueue::empty(IntPtr address) const
{
   AddressMap::const_iterator it = _address_map.find(address);
   return (it == _address_map.end());
}
