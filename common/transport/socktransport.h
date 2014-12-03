#ifndef SOCK_TRANSPORT_H
#define SOCK_TRANSPORT_H

#include "transport.h"
#include "thread.h"
#include "semaphore.h"

#include <queue>

class SockTransport : public Transport
{
public:
   SockTransport();
   ~SockTransport();
   
   class SockNode : public Node
   {
   public:
      SockNode(tile_id_t tile_id, SockTransport *trans);
      ~SockNode();

      void globalSend(SInt32 dest_proc, const void *buffer, UInt32 length);
      void send(tile_id_t dest_tile, const void *buffer, UInt32 length);
      Byte* recv();
      bool query();

   private:
      void send(SInt32 dest_proc, SInt32 tag, const void *buffer, UInt32 length);
      heap_id_t getHeapID() const;

      SockTransport *m_transport;
   };

   Node *createNode(tile_id_t tile_id);

   void barrier();
   Node *getGlobalNode();

private:
   struct Packet
   {
      UInt32 length;
      SInt32 tag;
   } __attribute__((packed));

   void getProcInfo();
   void initSockets();
   void initBufferLists();
   void insertInBufferList(SInt32 tag, Byte *buffer);

   static void updateThreadFunc(void *vp);
   void updateBufferLists();
   void terminateUpdateThread();

   class Socket
   {
   public:
      Socket();
      ~Socket();

      // server api
      void listen(SInt32 port, SInt32 max_pending);
      Socket accept();

      // client api
      void connect(const char *addr, SInt32 port);

      void send(const void* buffer, UInt32 length);
      bool recv(void *buffer, UInt32 length, bool block);

      void close();

   private:
      Socket(SInt32);

      SInt32 m_socket;
   };

   enum UpdateThreadState
   {
      RUNNING,
      EXITING
   };

   static const SInt32 GLOBAL_TAG = -1;
   static const SInt32 BARRIER_TAG = -2;
   static const SInt32 TERMINATE_TAG = -3;

   Node *m_global_node;

   SInt32 m_base_port;
   SInt32 m_num_procs;
   SInt32 m_num_targets;  //sqc_multi
   SInt32 m_proc_index;
   SInt32 m_target_index; //sqc_multi

   Semaphore m_barrier_sem;

   Socket m_server_socket;
   Lock *m_recv_locks;
   Socket *m_recv_sockets;
   Lock *m_send_locks;
   Socket *m_send_sockets;

   Thread *m_update_thread;
   UpdateThreadState m_update_thread_state;

   typedef std::queue<Byte*> buffer_list;
   SInt32 m_num_lists;
   buffer_list *m_buffer_lists;

   Lock *m_buffer_list_locks;
   Semaphore *m_buffer_list_sems;
};

#endif // SOCK_TRANSPORT_H
