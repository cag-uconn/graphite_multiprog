#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>

#include "log.h"
#include "config.h"
#include "simulator.h" //interface to config file singleton
#include "socktransport.h"

using std::string;

SockTransport::SockTransport()
{
   try
   {
      m_base_port = Sim()->getCfg()->getInt("transport/base_port");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read [transport/base_port] from the cfg file");
   }

   getProcInfo();
   initBufferLists();

   if (m_num_procs > 1)
   {
      // Initialize TCP/IP sockets
      initSockets();
      // Update thread for communicating between processes
      m_update_thread_state = RUNNING;
      m_update_thread = Thread::create(updateThreadFunc, this);
      m_update_thread->spawn();
   }

   m_global_node = new SockNode(GLOBAL_TAG, this);
}

void SockTransport::getProcInfo()  //sqc_multi 
{
   m_num_procs = (SInt32)Config::getSingleton()->getProcessCount();
   m_num_targets = (SInt32)Config::getSingleton()->getTargetCount();

   const char *proc_index_str = getenv("CARBON_PROCESS_INDEX");
   LOG_ASSERT_ERROR(proc_index_str != NULL || m_num_procs == 1,
                    "Process index undefined with multiple processes.");
					
   const char *target_index_str = getenv("CARBON_TARGET_INDEX");
   LOG_ASSERT_ERROR(target_index_str != NULL || m_num_targets == 1,
                    "Target index undefined with multiple targets.");
	  
   if (proc_index_str)
      m_proc_index = atoi(proc_index_str);
   else
      m_proc_index = 0;

   LOG_ASSERT_ERROR(0 <= m_proc_index && m_proc_index < m_num_procs,
                    "Invalid process index: %d with num_procs: %d", m_proc_index, m_num_procs);

   Config::getSingleton()->setProcessNum(m_proc_index);
   LOG_PRINT("Process number set to %i", Config::getSingleton()->getCurrentProcessNum());
   
   if (target_index_str)
      m_target_index = atoi(target_index_str);
   else
      m_target_index = 0;
	  
   LOG_ASSERT_ERROR(0 <= m_target_index && m_target_index < m_num_targets,
                    "Invalid target index: %d with num_targets: %d", m_target_index, m_num_targets);

   Config::getSingleton()->setTargetNum(m_target_index);
   LOG_PRINT("Target number set to %i", Config::getSingleton()->getCurrentTargetNum());

   // set host process number in this target
   char target_str[8];
   snprintf(target_str, 8, "%d", m_target_index);
   string target_string = "target_map/target";
   target_string += target_str;
   UInt32 num_procs_target = Sim()->getCfg()->getInt(target_string);
   
   Config::getSingleton()->setProcessCountCurrentTarget(num_procs_target);
   fprintf(stderr, "Number of process in target %i: (%i) \n",m_target_index, num_procs_target);
}

void SockTransport::initBufferLists()
{
   m_num_lists
      = Config::getSingleton()->getTotalTiles() // for tiles
      + 1; // for global node

   m_buffer_lists = new buffer_list[m_num_lists];

   m_buffer_list_locks = new Lock[m_num_lists];
   m_buffer_list_sems = new Semaphore[m_num_lists];
}

void SockTransport::initSockets() 
{
   SInt32 my_port;

   LOG_PRINT("initSockets()");

   // -- server side
   my_port = m_base_port + m_proc_index;
   m_server_socket.listen(my_port, m_num_procs);

   // -- client side
   m_send_sockets = new Socket[m_num_procs];
   m_send_locks = new Lock[m_num_procs];

   for (SInt32 proc = 0; proc < m_num_procs; proc++)
   {
      // Look up the mapping in the config file to find the address for this
      // particular process.
      char proc_str[8];
      snprintf(proc_str, 8, "%d", proc);
      string server_string = "process_map/process";
      server_string += proc_str;
      string server_addr = "";
      try
      {
          server_addr = Sim()->getCfg()->getString(server_string);
      } catch (...)
      {
          LOG_PRINT_ERROR("Key: %s not found in config!", server_string.c_str());
      }

      m_send_sockets[proc].connect(server_addr.c_str(), m_base_port + proc);

      m_send_sockets[proc].send(&m_proc_index, sizeof(m_proc_index));
   }

   // -- accept connections
   m_recv_sockets = new Socket[m_num_procs];
   m_recv_locks = new Lock[m_num_procs];

   for (SInt32 proc = 0; proc < m_num_procs; proc++)
   {
      Socket sock = m_server_socket.accept();

      SInt32 proc_index;
      sock.recv(&proc_index, sizeof(proc_index), true);

      LOG_ASSERT_ERROR(0 <= proc_index && proc_index < m_num_procs,
                       "Connected process out of range: %d",
                       proc_index);

      m_recv_sockets[proc_index] = sock;
   }
}

void SockTransport::updateThreadFunc(void *vp)
{
   LOG_PRINT("Starting updateThreadFunc");

   SockTransport *st = (SockTransport*)vp;

   while (st->m_update_thread_state == RUNNING)
   {
      st->updateBufferLists();
      sched_yield();
   }

   LOG_PRINT("Leaving updateThreadFunc");
}

void SockTransport::updateBufferLists()
{
   for (SInt32 i = 0; i < m_num_procs; i++)
   {
      while (true)
      {
         UInt32 length;

         m_recv_locks[i].acquire();

         // first get packet length, abort if none available
         if (!m_recv_sockets[i].recv(&length, sizeof(length), false))
         {
            m_recv_locks[i].release();
            break;
         }

         // now receive tag
         SInt32 tag;
         m_recv_sockets[i].recv(&tag, sizeof(tag), true);

         // now receive packet
         Byte *buffer = new(TRANSPORT_HEAP_ID) Byte[length];
         m_recv_sockets[i].recv(buffer, length, true);

         m_recv_locks[i].release();

         switch (tag)
         {
         case TERMINATE_TAG:
            LOG_PRINT("Quit message received.");
            LOG_ASSERT_ERROR(m_update_thread_state == RUNNING, "Terminate received in unexpected state: %d", m_update_thread_state);
            LOG_ASSERT_ERROR(i == m_proc_index, "Terminate received from unexpected process: %d != %d", i, m_proc_index);
            m_update_thread_state = EXITING;

            delete [] buffer;
            return;

         case BARRIER_TAG:
            m_barrier_sem.signal();
            LOG_ASSERT_ERROR(i == (m_proc_index + m_num_procs - 1) % m_num_procs,
                             "Barrier update from unexpected process: %d", i);
            delete [] buffer;
            break;

         case GLOBAL_TAG:
         default:
            insertInBufferList(tag, buffer);
            // do NOT delete buffer
            break;
         };
      }
   }
}

void SockTransport::insertInBufferList(SInt32 tag, Byte *buffer)
{
   if (tag == GLOBAL_TAG)
      tag = m_num_lists - 1;

   LOG_ASSERT_ERROR(0 <= tag && tag < m_num_lists, "Unexpected tag value: %d", tag);
   m_buffer_list_locks[tag].acquire();
   m_buffer_lists[tag].push(buffer);

   m_buffer_list_locks[tag].release();
   
   m_buffer_list_sems[tag].signal();
}

void SockTransport::terminateUpdateThread()
{
   LOG_PRINT("Sending quit message.");

   // include m_proc_index as a dummy message body just to avoid extra
   // code paths in updateBufferLists
   SInt32 quit_message[] = { sizeof(m_proc_index), TERMINATE_TAG, m_proc_index };
   m_send_sockets[m_proc_index].send(quit_message, sizeof(quit_message));

   m_update_thread->join();

   LOG_PRINT("Quit.");
}

SockTransport::~SockTransport()
{
   LOG_PRINT("dtor");

   delete m_global_node;

   if (m_num_procs > 1)
   {
      // Terminate update thread
      terminateUpdateThread();
      delete m_update_thread;

      // De-initialize sockets & locks
      for (SInt32 i = 0; i < m_num_procs; i++)
      {
         m_recv_sockets[i].close();
         m_send_sockets[i].close();
      }
      m_server_socket.close();
      
      delete [] m_recv_locks;
      delete [] m_recv_sockets;
      delete [] m_send_locks;
      delete [] m_send_sockets;
   }

   delete [] m_buffer_list_sems;
   delete [] m_buffer_list_locks;

   delete [] m_buffer_lists;

}

Transport::Node* SockTransport::createNode(tile_id_t tile_id)
{
   return new SockNode(tile_id, this);
}

void SockTransport::barrier()
{
   // We implement a barrier using a ring of messages. We are using a
   // single socket for the entire process, however, and it is
   // multiplexed between many tiles. So updates occur asynchronously
   // and possibly in other threads. That's what the semaphore takes
   // care of.

   //   There are two trips around the ring. The first trip blocks the
   // processes until everyone arrives. The second wakes them. This is
   // a low-performance implementation, but given how barriers are
   // used in the simulator, it should be OK. (Bear in mind this is
   // the Transport::barrier, NOT the CarbonBarrier implementation.)

   // Only required if the number of processes > 1
   if (m_num_procs == 1)
      return;
   
   LOG_PRINT("Entering transport barrier");

   Socket &sock = m_send_sockets[(m_proc_index+1) % m_num_procs];
   SInt32 message[] = { sizeof(SInt32), BARRIER_TAG, 0 };

   if (m_proc_index != 0)
      m_barrier_sem.wait();

   sock.send(message, sizeof(message));

   m_barrier_sem.wait();

   if (m_proc_index != m_num_procs - 1)
      sock.send(message, sizeof(message));

   LOG_PRINT("Exiting transport barrier");
}

Transport::Node* SockTransport::getGlobalNode()
{
   return m_global_node;
}

// -- SockTransport::SockNode

SockTransport::SockNode::SockNode(tile_id_t tile_id, SockTransport *trans)
   : Node(tile_id)
   , m_transport(trans)
{
}

SockTransport::SockNode::~SockNode()
{
}

void SockTransport::SockNode::globalSend(SInt32 dest_proc, 
                                         const void *buffer, 
                                         UInt32 length)
{
   send(dest_proc, GLOBAL_TAG, buffer, length);
}

void SockTransport::SockNode::send(tile_id_t dest_tile, 
                                   const void *buffer, 
                                   UInt32 length)
{
   int dest_proc = Config::getSingleton()->getProcessNumForTile(dest_tile);
   send(dest_proc, dest_tile, buffer, length);
}

Byte* SockTransport::SockNode::recv()
{
   LOG_PRINT("Entering recv");

   tile_id_t tag = getTileId();
   tag = (tag == GLOBAL_TAG) ? m_transport->m_num_lists - 1 : tag;
   
   m_transport->m_buffer_list_sems[tag].wait();

   Lock &lock = m_transport->m_buffer_list_locks[tag];
   lock.acquire();
   
   buffer_list &list = m_transport->m_buffer_lists[tag];
   LOG_ASSERT_ERROR(!list.empty(), "Buffer list empty after waiting on semaphore.");
   Byte* buffer = list.front();
   list.pop();

   lock.release();

   LOG_PRINT("Message recv'd");

   return buffer;
}

bool SockTransport::SockNode::query()
{
   tile_id_t tag = getTileId();
   tag = (tag == GLOBAL_TAG) ? m_transport->m_num_lists - 1 : tag;

   buffer_list &list = m_transport->m_buffer_lists[tag];
   Lock &lock = m_transport->m_buffer_list_locks[tag];

   lock.acquire();
   bool result = !list.empty();
   lock.release();
   return result;
}

void SockTransport::SockNode::send(SInt32 dest_proc, 
                                   SInt32 tag,
                                   const void *buffer, 
                                   UInt32 length)
{
   // two cases:
   // (1) remote process, use sockets
   // (2) single process, put directly in buffer list

   if (dest_proc == m_transport->m_proc_index)
   {
      Byte* buff_cpy = new(getHeapID()) Byte[length];
      memcpy(buff_cpy, buffer, length);

      m_transport->insertInBufferList(tag, buff_cpy);
   }
   else
   {
      SInt32 pkt_len = sizeof(length) + sizeof(tag) + length;

      Byte *pkt_buff = new(getHeapID()) Byte[pkt_len];

      // Length, Tag, Data, (Checksum)
      Packet *p = (Packet*)pkt_buff;
      
      p->length = length;
      p->tag = tag;
      memcpy(pkt_buff + sizeof(Packet), buffer, length);

      m_transport->m_send_locks[dest_proc].acquire();
      m_transport->m_send_sockets[dest_proc].send(pkt_buff, pkt_len);
      m_transport->m_send_locks[dest_proc].release();

      delete [] pkt_buff;
   }

   LOG_PRINT("Message sent.");
}

heap_id_t SockTransport::SockNode::getHeapID() const
{
   tile_id_t tag = getTileId();
   return (tag == GLOBAL_TAG) ? TRANSPORT_HEAP_ID : tag;
}

// -- Socket

SockTransport::Socket::Socket()
   : m_socket(-1)
{
}

SockTransport::Socket::Socket(SInt32 sock)
   : m_socket(sock)
{
}

SockTransport::Socket::~Socket()
{
}

void SockTransport::Socket::listen(SInt32 port, SInt32 max_pending)
{
   __attribute__((unused)) SInt32 err;

   LOG_ASSERT_ERROR(m_socket == -1, "Listening on already-open socket: %d", m_socket);

   m_socket = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
   LOG_ASSERT_ERROR(m_socket >= 0, "Failed to create socket.");

   SInt32 on = 1;
   err = ::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
   LOG_ASSERT_ERROR(err >= 0, "Failed to set socket options.");

   struct sockaddr_in addr;
   memset(&addr, 0, sizeof(addr));
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = INADDR_ANY;
   addr.sin_port = htons(port);

   err = ::bind(m_socket,
                (struct sockaddr *) &addr,
                sizeof(addr));
	LOG_ASSERT_ERROR(err >= 0, "Failed to bind");
   

   err = ::listen(m_socket,
                  max_pending);
   LOG_ASSERT_ERROR(err >= 0, "Failed to listen.");

   LOG_PRINT("Listening on socket: %d", m_socket);
}

SockTransport::Socket SockTransport::Socket::accept()
{
   struct sockaddr_in client;
   UInt32 client_len;
   SInt32 sock;

   memset(&client, 0, sizeof(client));
   client_len = sizeof(client);

   sock = ::accept(m_socket,
                   (struct sockaddr *)&client,
                   &client_len);
   LOG_ASSERT_ERROR(sock >= 0, "Failed to accept.");

   LOG_PRINT("Accepted connection %d on socket %d", sock, m_socket);

   return Socket(sock);
}

void SockTransport::Socket::connect(const char* addr, SInt32 port)
{
   SInt32 err;

   LOG_ASSERT_ERROR(m_socket == -1, "Connecting on already-open socket: %d", m_socket);
   
   // create socket
   m_socket = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
   LOG_ASSERT_ERROR(m_socket >= 0, "Failed to create socket -- addr: %s, port: %d.", addr, port);

   // lookup the hostname
   struct hostent *host;
   if ((host = gethostbyname(addr)) == NULL)
   {
      LOG_ASSERT_ERROR("Lookup on host: %s failed!", addr);
   }

   // connect
   struct sockaddr_in saddr;
   memset(&saddr, 0, sizeof(saddr));
   saddr.sin_family = AF_INET;
   memcpy(&(saddr.sin_addr.s_addr), host->h_addr_list[0], host->h_length);
   saddr.sin_port = htons(port);

   while (true)
   {
      err = ::connect(m_socket, (struct sockaddr *)&saddr, sizeof(saddr));

      //LOG_ASSERT_ERROR(err >= 0, "Failed to connect -- addr: %s, port: %d", addr, port);

      if (err >= 0)
         break;
      else
         sched_yield();
   }

   LOG_PRINT("Connected on socket %d to %s:%d", m_socket, addr, port);
}

void SockTransport::Socket::send(const void* buffer, UInt32 length)
{
   __attribute__((unused)) SInt32 sent;
   sent = ::send(m_socket, buffer, length, 0);
   LOG_ASSERT_ERROR(sent == SInt32(length), "Failure sending packet on socket %d -- %d != %d", m_socket, sent, length);
}

bool SockTransport::Socket::recv(void *buffer, UInt32 length, bool block)
{
   SInt32 recvd;

   while (true)
   {
      recvd = ::recv(m_socket, buffer, length, MSG_DONTWAIT);

      LOG_ASSERT_ERROR(recvd >= -1,
            "recvd(%i), length(%u), block(%u)", recvd, length, (UInt32) block);

      if (recvd >= 1)
      {
         buffer = (void*)((Byte*)buffer + recvd);
         length -= recvd;

         while (length > 0)
         {
            // block to receive remainder of message
            recvd = ::recv(m_socket, buffer, length, 0);
            LOG_ASSERT_ERROR(recvd >= 1 && recvd <= (SInt32) length,
                  "Error on socket(%i): recvd(%i), length(%u), block(%s)",
                  m_socket, recvd, length, block ? "Yes" : "No");

            buffer = (void*)((Byte*)buffer + recvd);
            length -= recvd;
         }
         return true;
      }
      else
      {
         if (!block)
         {
            return false;
         }
         else
         {
            sched_yield();
         }
      }
   }
}

void SockTransport::Socket::close()
{
   LOG_PRINT("Closing socket: %d", m_socket);

   __attribute__((unused)) SInt32 err;
   err = ::close(m_socket);
   LOG_ASSERT_WARNING(err >= 0, "Failed to close socket: %d", err);

   m_socket = -1;
}
