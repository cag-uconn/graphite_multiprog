#include <sys/syscall.h>
#include <cassert>
#include "thread_manager.h"
#include "thread_scheduler.h"
#include "tile_manager.h"
#include "config.h"
#include "log.h"
#include "transport.h"
#include "simulator.h"
#include "mcp.h"
#include "clock_skew_management_object.h"
#include "network.h"
#include "message_types.h"
#include "tile.h"
#include "core.h"
#include "core_model.h"
#include "thread.h"
#include "packetize.h"

ThreadManager::ThreadManager(TileManager *tile_manager)
   : m_thread_spawn_sem(0)
   , m_thread_spawn_lock()
   , m_tile_manager(tile_manager)
   , m_thread_spawners_terminated(0)
{
   Config *config = Config::getSingleton();

   m_master = config->isMasterProcess();

   m_tid_counter = 0;

   // Set the thread-spawner and MCP tiles to running.
   if (m_master)
   {
      UInt32 total_tiles = config->getTotalTilesCurrentTarget();
      UInt32 threads_per_core = config->getMaxThreadsPerCore();

      m_thread_state.resize(total_tiles);
      for (UInt32 i = 0; i < total_tiles; i++)
         m_thread_state[i].resize(threads_per_core);

      // Initialize all threads to IDLE and zero affinity masks
      for (UInt32 i = 0; i < total_tiles; i++) {
         for (UInt32 j = 0; j < threads_per_core; j++) {
            m_thread_state[i][j].status = Core::IDLE;
            m_thread_state[i][j].cpu_set = CPU_ALLOC(total_tiles);
            CPU_ZERO_S(CPU_ALLOC_SIZE(total_tiles), m_thread_state[i][j].cpu_set);
         }
      }

      // Get tile-ID for master thread
      tile_id_t master_tile_idx = getTileIDXFromTileID(config->getMasterThreadTileID());
      m_thread_state[master_tile_idx][0].status = Core::RUNNING;
            
      m_last_stalled_thread.resize(total_tiles);
      for (UInt32 i = 0; i < total_tiles; i++)
         m_last_stalled_thread[i] = INVALID_THREAD_ID;

      // Set Thread Spawner cores running
      if (config->getSimulationMode() == Config::FULL)
      {
         Config::TileList tile_ID_list = config->getThreadSpawnerTileIDList();
         for (Config::TileList::const_iterator itr = tile_ID_list.begin(); itr != tile_ID_list.end(); itr ++)
         {
            SInt32 tile_IDX = getTileIDXFromTileID(*itr);
            m_thread_state[tile_IDX][0] = Core::RUNNING;
         }
         //SInt32 first_thread_spawner_IDX = total_tiles - config->getProcessCountCurrentTarget() - 1;
         //SInt32 last_thread_spawner_IDX = total_tiles - 2;
         //for (SInt32 i = first_thread_spawner_IDX; i <= last_thread_spawner_IDX; i++)
         //   m_thread_state[i][0].status = Core::RUNNING;
      }
     
      // Set MCP core running
      SInt32 MCP_tile_idx = getTileIDXFromTileID(config->getMCPTileID());
      m_thread_state[MCP_tile_idx][0].status = Core::RUNNING;

      assert(total_tiles == m_thread_state.size());
   }
}

ThreadManager::~ThreadManager()
{
   if (m_master)
   {
      Config* config = Sim()->getConfig();

      // Set master tile ID to IDLE
      tile_id_t master_tile_idx = getTileIDXFromTileID(config->getMasterThreadTileID());
      m_thread_state[master_tile_idx][0].status = Core::IDLE;

      // Set MCP tile ID to IDLE
      SInt32 MCP_tile_idx = getTileIDXFromTileID(config->getMCPTileID());
      m_thread_state[MCP_tile_idx][0].status = Core::IDLE;
      
      if (config->getSimulationMode() == Config::FULL)
      {
         // Set Thread Spawner tile IDs to IDLE
         Config::TileList tile_ID_list = config->getThreadSpawnerTileIDList();
         for (Config::TileList::const_iterator itr = tile_ID_list.begin(); itr != tile_ID_list.end(); itr ++)
         {
            SInt32 tile_IDX = getTileIDXFromTileID(*itr);
            m_thread_state[tile_IDX][0] = Core::IDLE;
         }
      }
     
      // Check that all cores are IDLE 
      for (UInt32 i = 0; i < config->getApplicationTilesCurrentTarget(); i++)
      {
         for (UInt32 j = 0; j < config->getMaxThreadsPerCore(); j++)
         {
            LOG_ASSERT_ERROR(m_thread_state[i][j].status == Core::IDLE, "Thread %d on tile %d still active when ThreadManager destructs!", j, i);
            CPU_FREE(m_thread_state[i][j].cpu_set);
         }
      }
   }
}

void ThreadManager::onThreadStart(ThreadSpawnRequest *req)
{
   LOG_PRINT("onThreadStart[Tile-ID: %i, Thread-IDX: %i, Thread-ID: %i]",
         req->destination.tile_id, req->destination_tidx, req->destination_tid);

   m_tile_manager->initializeThread(req->destination, req->destination_tidx, req->destination_tid);
   assert(req->destination.tile_id == m_tile_manager->getCurrentCoreID().tile_id &&
          req->destination.core_type == m_tile_manager->getCurrentCoreID().core_type &&
          req->destination_tidx == m_tile_manager->getCurrentThreadIndex() &&
          req->destination_tid == m_tile_manager->getCurrentThreadID());

   if (req->destination.tile_id == Config::getSingleton()->getCurrentThreadSpawnerTileID())
      return;

   Core* core = m_tile_manager->getCurrentCore();
   assert(core->getState() == Core::IDLE || core->getState() == Core::STALLED);
   // Set the CoreState to 'RUNNING'
   core->setState(Core::RUNNING);

   // Set OS-TID (operating system - thread ID) of this thread
   setOSTid(req->destination, req->destination_tidx, syscall(SYS_gettid));

   // send message to master process to update global thread state 
   Network *net = core->getTile()->getNetwork();
   SInt32 msg[] = { MCP_MESSAGE_THREAD_START, core->getId().tile_id, core->getId().core_type, m_tile_manager->getCurrentThreadIndex()};
   net->netSend(Config::getSingleton()->getMCPCoreID(),
                MCP_REQUEST_TYPE,
                msg,
                sizeof(SInt32) + sizeof(core_id_t) + sizeof(thread_id_t));   //sqc_multi

   CoreModel *core_model = core->getModel();
   if (core_model)
   {
      core_model->processDynamicInstruction(new SpawnInstruction(Time(req->time)));
   }
}

void ThreadManager::masterOnThreadStart(tile_id_t tile_id, SInt32 core_type, SInt32 thread_idx)
{
   LOG_ASSERT_ERROR(m_master, "masterOnThreadStart() only called on master process");
   // Set the CoreState to 'RUNNING'
   SInt32 tile_idx = getTileIDXFromTileID(tile_id);
   LOG_ASSERT_ERROR(m_thread_state[tile_idx][thread_idx].status == Core::INITIALIZING,
                    "Thread on Tile-ID:%i, Tile-IDX:%i, should be in initializing state but isn't (state = %i)",
                    tile_id, tile_idx, m_thread_state[tile_idx][thread_idx].status);
   m_thread_state[tile_idx][thread_idx].status = Core::RUNNING;
}

void ThreadManager::onThreadExit()
{
   if (m_tile_manager->getCurrentTileID() == -1)
      return;
 
   Core* core = m_tile_manager->getCurrentCore();
   thread_id_t thread_idx = m_tile_manager->getCurrentThreadIndex();

   LOG_PRINT("onThreadExit -- send message to master ThreadManager; thread on Tile-ID:%i, Thread-IDX:%i at time %llu",
             core->getId().tile_id, thread_idx,
             core->getModel()->getCurrTime().toNanosec());
   Network *net = core->getTile()->getNetwork();

   // Recompute Average Frequency
   CoreModel* core_model = core->getModel();
   core_model->recomputeAverageFrequency(core->getFrequency());

   // send message to master process to update thread state
   SInt32 msg[] = { MCP_MESSAGE_THREAD_EXIT, core->getId().tile_id, core->getId().core_type, thread_idx };

   // update global thread state
   net->netSend(Config::getSingleton()->getMCPCoreID(),
                MCP_REQUEST_TYPE,
                msg,
                sizeof(SInt32) + sizeof(core_id_t) + sizeof(thread_id_t));

   m_thread_scheduler->onThreadExit();

   // Set the CoreState to 'IDLE'
   core->setState(Core::IDLE);

   if (config->getSimulationMode() == Config::FULL)
   {
      // Terminate thread spawners if master thread
      if (core->getId().tile_id == config->getMasterThreadTileID())
         terminateThreadSpawners();
   }

   // Terminate thread locally so we are ready for new thread requests on that tile
   m_tile_manager->terminateThread();

   LOG_PRINT("Finished onThreadExit: thread on Tile-ID:%i, Thread-IDX:%i", core->getId().tile_id, thread_idx);
}

void ThreadManager::masterOnThreadExit(tile_id_t tile_id, SInt32 core_type, SInt32 thread_idx, UInt64 time)
{
   LOG_PRINT("masterOnThreadExit: thread on Tile-ID:%i, Thread-IDX:%i", tile_id, thread_idx);
   LOG_ASSERT_ERROR(m_master, "masterOnThreadExit() only called on master process");
   
   core_id_t core_id = (core_id_t) {tile_id, core_type};
   SInt32 tile_idx = getTileIDXFromTileID(tile_id);
   LOG_ASSERT_ERROR((UInt32) tile_idx < m_thread_state.size(), "Tile-IDX:%i out of range", tile_idx);

   LOG_ASSERT_ERROR(m_thread_state[tile_idx][thread_idx].status == Core::RUNNING,
                    "Exiting: thread on Tile-ID:%i, Thread-IDX(%d) is NOT running", tile_id, thread_idx);
   m_thread_state[tile_idx][thread_idx].status = Core::IDLE;
   m_thread_state[tile_idx][thread_idx].completion_time = Time(time);

   if (Sim()->getMCP()->getClockSkewManagementServer())
      Sim()->getMCP()->getClockSkewManagementServer()->signal();

   // Wake up any threads that is waiting on this thread to finish
   wakeUpWaiter(core_id, thread_idx, Time(time));

   m_thread_scheduler->masterOnThreadExit(core_id, thread_idx);

   if (Config::getSingleton()->getSimulationMode() == Config::FULL)
      slaveTerminateThreadSpawnerAck(tile_id);
}

/*
  Thread spawning occurs in the following steps:

  1. A message is sent from requestor to the master thread manager.
  2. The master thread manager finds the destination core and asks the master thread scheduler to schedule the thread.
  3. The master thread scheduler enters the thread into a queue, and decides if the core is idle and contacts the host process if so.
  4. The master thread manager sends an ACK back to the thread that called for the new thread, and it replies with the tid of the newly spawned thread.
  5. The host process spawns the new thread by adding the information to a list and posting a sempaphore.
  6. The thread is spawned by the thread spawner coming from the application.
  7. The spawned thread is picked up by the local thread spawner and initializes.
*/

SInt32 ThreadManager::spawnThread(tile_id_t dest_tile_id, thread_func_t func, void *arg)
{
   // step 1
   LOG_PRINT("(1) spawnThread with func: %p and arg: %p", func, arg);

   Core *core = m_tile_manager->getCurrentCore();
   thread_id_t thread_idx = m_tile_manager->getCurrentThreadIndex();
   Network *net = core->getTile()->getNetwork();

   Time curr_time = core->getModel()->getCurrTime();
   
   core->setState(Core::STALLED);

   core_id_t dest_core_id = INVALID_CORE_ID;
   // If destination was specified, send it there.  Otherwise pick a free core at the MCP.
   if (dest_tile_id != INVALID_TILE_ID)
      dest_core_id = Tile::getMainCoreId(dest_tile_id);

   ThreadSpawnRequest req = { MCP_MESSAGE_THREAD_SPAWN_REQUEST_FROM_REQUESTER,
                              func, arg,
                              core->getId(), thread_idx,
                              dest_core_id, INVALID_THREAD_ID, INVALID_THREAD_ID,
                              curr_time.getTime() };

   core_id_t mcp_core_id = Config::getSingleton()->getMCPCoreID();
   net->netSend(mcp_core_id,
                MCP_REQUEST_TYPE,
                &req,
                sizeof(req));

   NetPacket pkt = net->netRecvType(MCP_THREAD_SPAWN_REPLY_FROM_MASTER_TYPE, core->getId());
   
   LOG_ASSERT_ERROR(pkt.length == (sizeof(core_id_t) + sizeof(thread_id_t) + sizeof(thread_id_t)),
         "Unexpected reply size: pkt_length(%u), expected(%u)",
         pkt.length, sizeof(core_id_t) + sizeof(thread_id_t) + sizeof(thread_id_t));

   // Set the CoreState to 'RUNNING'
   core->setState(Core::RUNNING);

   dest_core_id = *(core_id_t*)((Byte*) pkt.data);
   dest_tile_id = dest_core_id.tile_id;
   __attribute__((unused)) thread_id_t dest_thread_idx = *(thread_id_t*) ((Byte*) pkt.data + sizeof(core_id_t));
   thread_id_t dest_thread_id = *(thread_id_t*) ((Byte*) pkt.data + sizeof(core_id_t) + sizeof(thread_id_t));
   LOG_PRINT("Thread:%i spawned on Tile-ID:%i, Thread-IDX:%i", dest_thread_id, dest_tile_id, dest_thread_idx);

   // Delete the data buffer
   delete [] (Byte*) pkt.data;

   return dest_thread_id;
}

void ThreadManager::masterSpawnThread(ThreadSpawnRequest *req)
{
   // step 2
   LOG_ASSERT_ERROR(m_master, "masterSpawnThread() should only be called on master.");
   LOG_PRINT("(2) masterSpawnThread with req: { Func:%p, Arg:%p, Req-Tile-ID:%i, Dest-Tile-ID:%i }",
         req->func, req->arg,
         req->requester.tile_id, req->destination.tile_id);

   // Find core to use
   // FIXME: Load balancing?
   Config* config = Config::getSingleton();
   UInt32 num_application_tiles = config->getApplicationTilesCurrentTarget();
   stallThread(req->requester, req->requester_tidx);

   // Tile-IDX on which thread is going to get spawned
   SInt32 destination_tile_idx = INVALID_TILE_IDX;
   SInt32 requester_tile_idx = getTileIDXFromTileID(req->requester.tile_id);
   if (req->destination.tile_id == INVALID_TILE_ID)
   {
      // Threads are strided across cores.
      for (SInt32 j = 0; j < (SInt32) config->getMaxThreadsPerCore(); j++)
      {
         for (SInt32 i = 1; i <= (SInt32) num_application_tiles; i++)
         {
            destination_tile_idx = (requester_tile_idx + i) % num_application_tiles;

            // FIXME: Master Tile can't be multithreaded.
            if (destination_tile_idx == 0)
               continue;

            if (m_thread_state[destination_tile_idx][j].status == Core::IDLE)
            {
               tile_id_t destination_tile_id = getTileIDFromTileIDX(destination_tile_idx);
               req->destination =  Tile::getMainCoreId(destination_tile_id);
               req->destination_tidx =  j;
               break;
            }
         }
      }
   }
   else // (req->destination.tile_id != INVALID_TILE_ID)
   {
      // Find a free slot.
      req->destination_tidx = getIdleThread(req->destination);
   }


   LOG_ASSERT_ERROR(req->destination.tile_id != INVALID_TILE_ID, "No cores available for spawnThread request.");
   LOG_ASSERT_ERROR(req->destination_tidx != INVALID_THREAD_ID, "No threads available on destination core for spawnThread request.");

   req->destination_tid = getNewThreadId(req->destination, req->destination_tidx);
   LOG_ASSERT_ERROR(req->destination_tid != INVALID_THREAD_ID, "Problem generating new thread id.");

   m_thread_scheduler->masterScheduleThread(req);

   // Tell the thread that issued the spawn request that it is done.
   LOG_PRINT("masterSpawnThread -- send ack to master: Func:%p, Arg:%p, "
             "Requester[Tile-ID:%i, Thread-IDX:%i], Destination[Tile-ID:%i, Thread-IDX:%i, Thread-ID:%i]",
             req->func, req->arg, req->requester.tile_id, req->requester_tidx,
             req->destination.tile_id, req->destination_tidx, req->destination_tid);

   masterSpawnThreadReply(req);
}

void ThreadManager::slaveSpawnThread(ThreadSpawnRequest *req)
{
   LOG_PRINT("(5) slaveSpawnThread with req: { Func:%p, Arg:%p, Requester[Tile-ID:%i, Thread-IDX:%i], Destination[Tile-ID:%i, Thread-IDX:%i, Thread-ID:%i] }",
             req->func, req->arg, req->requester.tile_id, req->requester_tidx, req->destination.tile_id, req->destination_tidx, req->destination_tid);

   // This is deleted after the thread has been spawned and we have sent the acknowledgement back to the requester
   ThreadSpawnRequest *req_cpy = new ThreadSpawnRequest;
   *req_cpy = *req;

   // Insert the request in the thread request queue and the thread request map
   insertThreadSpawnRequest (req_cpy);

   m_thread_spawn_sem.signal();
}

void ThreadManager::insertThreadSpawnRequest(ThreadSpawnRequest *req)
{
   // Insert the request in the thread request queue
   m_thread_spawn_lock.acquire();
   m_thread_spawn_list.push(req);
   m_thread_spawn_lock.release();
}

void ThreadManager::getThreadToSpawn(ThreadSpawnRequest *req)
{
   // step 6 - this is called from the thread spawner
   LOG_PRINT("(6a) getThreadToSpawn called by user.");
   
   // Wait for a request to arrive
   m_thread_spawn_sem.wait();
   
   // Grab the request and set the argument
   // The lock is released by the spawned thread
   m_thread_spawn_lock.acquire();
   *req = *((ThreadSpawnRequest*) m_thread_spawn_list.front());
   
   LOG_PRINT("(6b) getThreadToSpawn giving thread %p arg: %p to user.", req->func, req->arg);
}

ThreadSpawnRequest* ThreadManager::getThreadSpawnReq()
{
   if (m_thread_spawn_list.empty())
   {
      return (ThreadSpawnRequest*) NULL;
   }
   else
   {
      return m_thread_spawn_list.front();
   }
}

void ThreadManager::dequeueThreadSpawnReq (ThreadSpawnRequest *req)
{
   ThreadSpawnRequest *thread_req = m_thread_spawn_list.front();

   *req = *thread_req;

   m_thread_spawn_list.pop();

   m_thread_spawn_lock.release();

   delete thread_req;

   LOG_PRINT("Dequeued req: { Func:%p, Arg:%p, Requester[Tile-ID:%i, Thread-IDX:%i], Destination[Tile-ID:%i, Thread-IDX:%i, Thread-ID:%i] }",
             req->func, req->arg, req->requester.tile_id, req->requester_tidx, req->destination.tile_id, req->destination_tidx, req->destination_tid);
}

void ThreadManager::masterSpawnThreadReply(ThreadSpawnRequest *req)
{
   LOG_ASSERT_ERROR(m_master, "masterSpawnThreadReply() should only be called on master.");
   LOG_PRINT("(4) masterSpawnThreadReply with req: { Func:%p, Arg:%p, Requester[Tile-ID:%i, Thread-IDX:%i], Destination[Tile-ID:%i, Thread-IDX:%i, Thread-ID:%i] }",
             req->func, req->arg, req->requester.tile_id, req->requester_tidx, req->destination.tile_id, req->destination_tidx, req->destination_tid);

   // Resume the requesting thread
   LOG_PRINT("masterSpawnThreadReply resuming thread on Tile-ID:%i, Thread-IDX:%i",
             req->requester.tile_id, req->requester_tidx);
   resumeThread(req->requester);

   SInt32 msg[] = { req->destination.tile_id, req->destination.core_type, req->destination_tidx, req->destination_tid };

   Core *core = m_tile_manager->getCurrentCore();
   core->getTile()->getNetwork()->netSend(req->requester, 
         MCP_THREAD_SPAWN_REPLY_FROM_MASTER_TYPE,
         msg,
         sizeof(req->destination.tile_id)+sizeof(req->destination.core_type)+sizeof(req->destination_tidx)+sizeof(req->destination_tid));
}

void ThreadManager::joinThread(thread_id_t join_thread_id)
{
   // Send the message to the master process; will get reply when thread is finished
   Core* core = m_tile_manager->getCurrentCore();
   SInt32 thread_idx = m_tile_manager->getCurrentThreadIndex();

   ThreadJoinRequest msg = { MCP_MESSAGE_THREAD_JOIN_REQUEST,
                             core->getId(), thread_idx,
                             join_thread_id
                           };

   Network *net = core->getTile()->getNetwork();
   net->netSend(Config::getSingleton()->getMCPCoreID(),
                MCP_REQUEST_TYPE,
                &msg,
                sizeof(msg));

   // Set the CoreState to 'STALLED'
   core->setState(Core::STALLED);

   // Wait for reply
   NetPacket pkt = net->netRecvType(MCP_THREAD_JOIN_REPLY, core->getId());

   // Set the CoreState to 'WAKING_UP'
   core->setState(Core::WAKING_UP);
}

void ThreadManager::masterJoinThread(ThreadJoinRequest *req, Time time)
{
   // FIXME: BUG in this code
   //  Joining should be based purely on Thread-ID and not [Tile-ID,Thread-IDX]
   //  In this code, the thread that is joined could already be done but if another
   //     thread has been spawned on the same [Tile-ID,Thread-IDX] tuple, the requester
   //     will keep waiting on it, while ideally, it should return immediately.
   //  completion_time: Should be maintained on a per-thread basis.

   LOG_ASSERT_ERROR(m_master, "masterJoinThread() should only be called on master.");
   LOG_PRINT("masterJoinThread called for thread: %i", req->receiver_tid);

   thread_id_t join_thread_id = req->receiver_tid;
   LOG_ASSERT_ERROR(join_thread_id < (int) m_tid_to_core_map.size(), "A thread with ID(%i) has NOT been spawned", join_thread_id);

   core_id_t join_core_id = m_tid_to_core_map[join_thread_id].first;
   thread_id_t join_thread_idx =  m_tid_to_core_map[join_thread_id].second;

   // joinThread joins the current thread with the MAIN core's thread at tile_id. 
   LOG_PRINT("Joining thread [%d, Tile-ID:%i, Thread-IDX:%i], Requester[Tile-ID:%i, Thread-IDX:%i]",
             join_thread_id, join_core_id.tile_id, join_thread_idx,
             req->sender.tile_id, req->sender_tidx);

   SInt32 join_tile_idx = getTileIDXFromTileID(join_core_id.tile_id);
   LOG_ASSERT_ERROR(m_thread_state[join_tile_idx][join_thread_idx].waiter_core.tile_id == INVALID_TILE_ID,
                    "Multiple threads waiting on tile %d", join_core_id.tile_id);

   LOG_ASSERT_ERROR((UInt32) join_tile_idx < m_thread_state.size(), "Tile IDX:%i out of range", join_tile_idx);
   
   m_thread_state[join_tile_idx][join_thread_idx].waiter_core = req->sender;
   m_thread_state[join_tile_idx][join_thread_idx].waiter_tidx = req->sender_tidx;

   // Stall the 'pthread_join/CarbonJoinThread' caller
   stallThread(req->sender, req->sender_tidx);

   // Tile not running, so the thread must have joined
   if (m_thread_state[join_tile_idx][join_thread_idx].status == Core::IDLE)
   {
      LOG_PRINT("Not running, sending reply.");
      Time join_thread_completion_time = m_thread_state[join_tile_idx][join_thread_idx].completion_time;
      wakeUpWaiter(join_core_id, join_thread_idx, join_thread_completion_time);
   }
}

bool ThreadManager::wakeUpWaiter(core_id_t core_id, thread_id_t thread_idx, Time time)
{
   bool woken_up = false;
  
   SInt32 tile_idx = getTileIDXFromTileID(core_id.tile_id); 
   core_id_t waiter_core_id = m_thread_state[tile_idx][thread_idx].waiter_core;
   if (waiter_core_id.tile_id != INVALID_TILE_ID)
   {
      SInt32 waiter_thread_idx = m_thread_state[tile_idx][thread_idx].waiter_tidx;
      LOG_PRINT("Waking up thread on Tile:%i, Thread-IDX:%i at Time: %llu ns",
                waiter_core_id.tile_id, waiter_thread_idx, time.toNanosec());

      // Resume the 'pthread_join' caller
      LOG_PRINT("wakeUpWaiter resuming thread on Tile:%i, Thread-IDX:%i", waiter_core_id.tile_id, waiter_thread_idx);
      resumeThread(waiter_core_id, waiter_thread_idx);

      // we have to manually send a packet because we are manufacturing a time-stamp
      Core *core = m_tile_manager->getCurrentCore();
      NetPacket pkt(time,
                    MCP_THREAD_JOIN_REPLY,
                    core->getId(),
                    waiter_core_id,
                    0,
                    NULL);
      core->getTile()->getNetwork()->netSend(pkt);

      m_thread_state[tile_idx][thread_idx].waiter_core = INVALID_CORE_ID;
      m_thread_state[tile_idx][thread_idx].waiter_tidx = INVALID_THREAD_IDX;

      woken_up = true;
   }
   LOG_PRINT("Exiting wakeUpWaiter");
   return woken_up;
}

void ThreadManager::terminateThreadSpawners()
{
   LOG_PRINT ("In terminateThreadSpawner");

   Transport::Node *node = Transport::getSingleton()->getGlobalNode();

   assert(node);

   Config::ProcessList process_num_list = config->getProcessNumList();
   for (Config::ProcessList::const_iterator itr = process_num_list.begin(); itr != process_num_list.end(); itr ++)
   {
      SInt32 pid = *itr;
      int send_req = LCP_MESSAGE_QUIT_THREAD_SPAWNER;
      LOG_PRINT ("Sending thread spawner quit message to proc %d", pid);
      node->globalSend(pid, &send_req, sizeof (send_req));
      LOG_PRINT ("Sent thread spawner quit message to proc %d", pid);
   }

   // wait for all thread spawners to terminate
   while (true)
   {
      {
         ScopedLock sl(m_thread_spawners_terminated_lock);
         if (m_thread_spawners_terminated == process_num_list.size())
            break;
      }
      sched_yield();
   }
}

void ThreadManager::slaveTerminateThreadSpawner()
{
   LOG_PRINT("slaveTerminateThreadSpawner on proc %d", Config::getSingleton()->getCurrentProcessNum());

   ThreadSpawnRequest *req = new ThreadSpawnRequest;

   req->msg_type = LCP_MESSAGE_QUIT_THREAD_SPAWNER;
   req->func = NULL;
   req->arg = NULL;
   req->requester = INVALID_CORE_ID;
   req->destination = INVALID_CORE_ID;

   insertThreadSpawnRequest(req);
   m_thread_spawn_sem.signal();
}

void ThreadManager::slaveTerminateThreadSpawnerAck(tile_id_t tile_id)
{
   Config *config = Config::getSingleton();
   Config::ProcessList process_num_list = config->getProcessNumList();
   for (Config::ProcessList::iterator itr = process_num_list.begin(); itr != process_num_list.end(); itr ++)
   {
      if (tile_id == config->getThreadSpawnerTileID(*itr))
      {
         Transport::Node *node = m_tile_manager->getCurrentTile()->getNetwork()->getTransport();
         int req_type = LCP_MESSAGE_QUIT_THREAD_SPAWNER_ACK;
         node->globalSend(config->getMasterProcessNum(), &req_type, sizeof(req_type));
      }
   }
}

void ThreadManager::updateTerminateThreadSpawner()
{
   Config *config = Config::getSingleton();
   assert(config->isMasterProcess());
   ScopedLock sl(m_thread_spawners_terminated_lock);
   ++m_thread_spawners_terminated;
}

thread_id_t ThreadManager::getNewThreadId(core_id_t core_id, thread_id_t thread_idx)
{
   LOG_ASSERT_ERROR(m_master, "getNewThreadId() should only be called on master.");
   m_tid_counter_lock.acquire();

   m_tid_counter++;
   thread_id_t new_thread_id = m_tid_counter;

   if (m_tid_to_core_map.size() <= (UInt32) new_thread_id)
      m_tid_to_core_map.resize(2 * new_thread_id);

   m_tid_to_core_map[new_thread_id] = std::make_pair(core_id, thread_idx);

   LOG_ASSERT_ERROR(Tile::isMainCore(core_id), "Invalid core type!");
   SInt32 tile_idx = getTileIDXFromTileID(core_id.tile_id);
   m_thread_state[tile_idx][thread_idx].thread_id = new_thread_id;

   LOG_PRINT("Generated Thread-ID:%i for Thread on Tile:%i, Thread-Index:%i",
             new_thread_id, core_id.tile_id, thread_idx);
   m_tid_counter_lock.release();

   return new_thread_id;
}

void ThreadManager::setOSTid(core_id_t core_id, thread_id_t thread_idx, pid_t os_tid)
{
   LOG_PRINT("ThreadManager::setOSTid() called for Thread on Tile:%i, IDX:%i with OS-TID:%i",
             core_id.tile_id, thread_idx, os_tid);
   
   m_tid_map_lock.acquire();
   SInt32 req[] = { MCP_MESSAGE_THREAD_SET_OS_TID,
                    core_id.tile_id,
                    core_id.core_type,
                    thread_idx,
                    os_tid };

   Network *net = m_tile_manager->getCurrentCore()->getTile()->getNetwork();

   net->netSend(Config::getSingleton()->getMCPCoreID(),
                MCP_REQUEST_TYPE,
                &req,
                sizeof(req));

   m_tid_map_lock.release();
}

void ThreadManager::masterSetOSTid(tile_id_t tile_id, thread_id_t thread_idx, pid_t os_tid)
{
   LOG_ASSERT_ERROR(m_master, "masterSetOSTid() should only be called on master.");
   SInt32 tile_idx = getTileIDXFromTileID(tile_id);
   m_thread_state[tile_idx][thread_idx].os_tid = os_tid;
}

// Returns the thread INDEX and the core_id (by reference).
void ThreadManager::lookupThreadIndex(thread_id_t thread_id, core_id_t &core_id, thread_id_t &thread_idx)
{
   LOG_ASSERT_ERROR(m_master, "lookupThreadIndex() must only be called on master");
   LOG_ASSERT_ERROR(thread_id <= m_tid_counter, "A thread with TID:%i has not been spawned before, Maximum:%i", thread_id, m_tid_counter);
   m_tid_counter_lock.acquire();

   core_id = m_tid_to_core_map[thread_id].first;
   thread_idx = m_tid_to_core_map[thread_id].second;

   m_tid_counter_lock.release();
}

// Sets the thread INDEX and the core_id
void ThreadManager::setThreadIndex(thread_id_t thread_id, core_id_t core_id, thread_id_t thread_idx)
{
   LOG_ASSERT_ERROR(m_master, "setThreadIdx() must only be called on master");
   LOG_ASSERT_ERROR(thread_id <= m_tid_counter, "A thread with TID:%i has not been spawned before, Maximum:%i", thread_id, m_tid_counter);
   m_tid_counter_lock.acquire();

   m_tid_to_core_map[thread_id].first = core_id;
   m_tid_to_core_map[thread_id].second = thread_idx;  

   m_tid_counter_lock.release();
}

UInt32 ThreadManager::getNumScheduledThreads(core_id_t core_id)
{
   LOG_ASSERT_ERROR(m_master, "getNumScheduledThreads() must only be called on master");

   UInt32 num_idle_threads = 0;
   Config* config = Config::getSingleton();

   LOG_ASSERT_ERROR(Tile::isMainCore(core_id), "Invalid core type!");
   // Find a free thread.
   for (SInt32 j = 0; j < (SInt32) config->getMaxThreadsPerCore(); j++)
   {
      SInt32 tile_idx = getTileIDXFromTileID(core_id.tile_id);
      if (m_thread_state[tile_idx][j].status == Core::IDLE)
         num_idle_threads++;
   }

   return config->getMaxThreadsPerCore() - num_idle_threads;
}

SInt32 ThreadManager::getIdleThread(core_id_t core_id)
{
   LOG_ASSERT_ERROR(m_master, "getIdleThread() must only be called on master");

   SInt32 thread_idx = INVALID_THREAD_IDX;
   Config* config = Config::getSingleton();

   LOG_ASSERT_ERROR(Tile::isMainCore(core_id), "Invalid core type!");
   // Find a free thread.
   for (SInt32 j = 0; j < (SInt32) config->getMaxThreadsPerCore(); j++)
   {
      SInt32 tile_idx = getTileIDXFromTileID(core_id.tile_id);
      if (m_thread_state[tile_idx][j].status == Core::IDLE)
      {
         thread_idx = j;
         break;
      }
   }

   return thread_idx;
}

// Stalls the current running thread.
void ThreadManager::stallThread(core_id_t core_id)
{
   LOG_ASSERT_ERROR(m_master, "stallThread() must only be called on master");
   SInt32 thread_idx = getRunningThreadIDX(core_id);
   LOG_ASSERT_ERROR(thread_idx != INVALID_THREAD_IDX, "Can't stall thread on Tile:%i, NO running threads!", core_id.tile_id);
   stallThread(core_id, thread_idx);
}

void ThreadManager::stallThread(core_id_t core_id, SInt32 thread_idx)
{
   LOG_ASSERT_ERROR(m_master, "stallThread() must only be called on master");
   LOG_ASSERT_ERROR(Tile::isMainCore(core_id), "Invalid core type!");
   stallThread(core_id.tile_id, thread_idx);
}

void ThreadManager::stallThread(tile_id_t tile_id, SInt32 thread_idx)
{
   SInt32 tile_idx = getTileIDXFromTileID(tile_id);
   LOG_ASSERT_ERROR(m_thread_state[tile_idx][thread_idx].status == Core::RUNNING, "Thread on Tile:%i, IDX:%i not running", tile_id, thread_idx);
   m_thread_state[tile_idx][thread_idx].status = Core::STALLED;
   m_last_stalled_thread[tile_idx] = thread_idx;
   
   if (Sim()->getMCP()->getClockSkewManagementServer())
      Sim()->getMCP()->getClockSkewManagementServer()->signal();
}

void ThreadManager::resumeThread(core_id_t core_id)
{
   LOG_ASSERT_ERROR(m_master, "resumeThread() must only be called on master");
   LOG_ASSERT_ERROR(Tile::isMainCore(core_id), "Invalid core type!");
   
   SInt32 tile_idx = getTileIDXFromTileID(core_id.tile_id);
   SInt32 thread_idx = m_last_stalled_thread[tile_idx];
   assert(thread_idx != INVALID_THREAD_ID);

   resumeThread(core_id.tile_id, thread_idx);
}

void ThreadManager::resumeThread(core_id_t core_id, SInt32 thread_idx)
{
   LOG_ASSERT_ERROR(Tile::isMainCore(core_id), "Invalid core type!");
   resumeThread(core_id.tile_id, thread_idx);
}

void ThreadManager::resumeThread(tile_id_t tile_id, SInt32 thread_idx)
{
   LOG_ASSERT_ERROR(m_master, "resumeThread() must only be called on master");
   SInt32 tile_idx = getTileIDXFromTileID(tile_id);
   m_thread_state[tile_idx][thread_idx].status = Core::RUNNING;
}

bool ThreadManager::isThreadRunning(core_id_t core_id, SInt32 thread_idx)
{
   LOG_ASSERT_ERROR(Tile::isMainCore(core_id), "Invalid core type!");
   return isThreadRunning(core_id.tile_id, thread_idx);
}

bool ThreadManager::isThreadRunning(tile_id_t tile_id, SInt32 thread_idx)
{
   LOG_ASSERT_ERROR(m_master, "isThreadRunning() must only be called on master");
   SInt32 tile_idx = getTileIDXFromTileID(tile_id);
   return (m_thread_state[tile_idx][thread_idx].status == Core::RUNNING);
}

bool ThreadManager::isThreadInitializing(core_id_t core_id, SInt32 thread_idx)
{
   LOG_ASSERT_ERROR(Tile::isMainCore(core_id), "Invalid core type!");
   return isThreadInitializing(core_id.tile_id, thread_idx);
}

bool ThreadManager::isThreadInitializing(tile_id_t tile_id, SInt32 thread_idx)
{
   LOG_ASSERT_ERROR(m_master, "isThreadInitializing() must only be called on master");
   SInt32 tile_idx = getTileIDXFromTileID(tile_id);
   return (m_thread_state[tile_idx][thread_idx].status == Core::INITIALIZING);
}

bool ThreadManager::isCoreInitializing(core_id_t core_id)
{
   LOG_ASSERT_ERROR(m_master, "isCoreInitializing() must only be called on master");
   LOG_ASSERT_ERROR(Tile::isMainCore(core_id), "Invalid core type!");
   return isCoreInitializing(core_id.tile_id);
}

bool ThreadManager::isCoreInitializing(tile_id_t tile_id)
{
   LOG_ASSERT_ERROR(m_master, "isCoreInitializing() must only be called on master");
   SInt32 tile_idx = getTileIDXFromTileID(tile_id);
   for (SInt32 j = 0; j < (SInt32) m_thread_state[tile_idx].size(); j++)
   {
      if (isThreadInitializing(tile_id, j))
         return true;
   }
   return false;
}

bool ThreadManager::isThreadStalled(core_id_t core_id, SInt32 thread_idx)
{
   LOG_ASSERT_ERROR(Tile::isMainCore(core_id), "Invalid core type!");
   return isThreadStalled(core_id.tile_id, thread_idx);
}

bool ThreadManager::isThreadStalled(tile_id_t tile_id, thread_id_t thread_idx)
{
   LOG_ASSERT_ERROR(m_master, "isThreadStalled() must only be called on master");
   SInt32 tile_idx = getTileIDXFromTileID(tile_id);
   return (m_thread_state[tile_idx][thread_idx].status == Core::STALLED);
}

bool ThreadManager::isCoreStalled(core_id_t core_id)
{
   LOG_ASSERT_ERROR(Tile::isMainCore(core_id), "Invalid core type!");
   return isCoreStalled(core_id.tile_id);
}

bool ThreadManager::isCoreStalled(tile_id_t tile_id)
{
   LOG_ASSERT_ERROR(m_master, "isCoreStalled() must only be called on master");
   SInt32 tile_idx = getTileIDXFromTileID(tile_id);
   for (SInt32 j = 0; j < (SInt32) m_thread_state[tile_idx].size(); j++)
   {
      if (isThreadStalled(tile_id, j))
         return true;
   }
   return false;
}

bool ThreadManager::areAllCoresRunning()
{
   LOG_ASSERT_ERROR(m_master, "areAllCoresRunning() should only be called on master.");

   // Check if all the cores are running
   bool is_all_running = true;
   SInt32 thread_idx = INVALID_THREAD_IDX;
   for (SInt32 tile_idx = 0; tile_idx < (SInt32) m_thread_state.size(); tile_idx++)
   {
      tile_id_t tile_id = getTileIDFromTileIDX(tile_idx);
      if (getRunningThreadIDX(tile_id) == INVALID_THREAD_IDX)
         return false;
   }
   return true;
}

// Returns INVALID_THREAD_IDX if no threads are running
SInt32 ThreadManager::getRunningThreadIDX(core_id_t core_id)
{
   LOG_ASSERT_ERROR(Tile::isMainCore(core_id), "Invalid core type!");
   return getRunningThreadIDX(core_id.tile_id);
}

SInt32 ThreadManager::getRunningThreadIDX(tile_id_t tile_id)
{
   LOG_ASSERT_ERROR(m_master, "getRunningThreadIDX() must only be called on master");
   SInt32 thread_idx = INVALID_THREAD_IDX;
   SInt32 tile_idx = getTileIDXFromTileID(tile_id);
   for (SInt32 j = 0; j < (SInt32) m_thread_state[tile_idx].size(); j++)
   {
      if (isThreadRunning(tile_idx, j))
      {
         if (thread_idx == INVALID_THREAD_IDX)
            thread_idx = j;
         else
            LOG_PRINT_ERROR("Two threads on Tile:%i, IDX1:%i, IDX2:%i are running simultaneously", tile_id, thread_idx, j);
      }
   }

   return thread_idx;
}

int ThreadManager::setThreadAffinity(pid_t os_tid, cpu_set_t* set)
{
   thread_id_t thread_idx = INVALID_THREAD_ID;
   tile_id_t tile_id = INVALID_TILE_ID;
   int res = -1;

   for (SInt32 i = 0; i < (SInt32) m_thread_state.size(); i++)
   {
      for (SInt32 j = 0; j < (SInt32) m_thread_state[i].size(); j++)
      {
         if (m_thread_state[i][j].os_tid == os_tid)
         {
            if(thread_idx == INVALID_THREAD_ID)
            {
               tile_id = i;
               thread_idx = j;
            }
            else
               LOG_PRINT_ERROR("Two threads %i on %i and %i on %i have the same os_tid %i!", thread_idx, tile_id, j, i, os_tid);
         }
      }
   }

   if (thread_idx != INVALID_THREAD_ID)
   {
      res = 0;
      setThreadAffinity(tile_id, thread_idx, set);
   }

   return res;
}


void ThreadManager::setThreadAffinity(tile_id_t tile_id, thread_id_t tidx, cpu_set_t* set)
{
   CPU_ZERO_S(CPU_ALLOC_SIZE(Config::getSingleton()->getTotalTiles()), m_thread_state[tile_id][tidx].cpu_set);
   CPU_OR_S(CPU_ALLOC_SIZE(Config::getSingleton()->getTotalTiles()), m_thread_state[tile_id][tidx].cpu_set, set, set);
}

int ThreadManager::getThreadAffinity(pid_t os_tid, cpu_set_t* set)
{
   thread_id_t thread_idx = INVALID_THREAD_ID;
   tile_id_t tile_id = INVALID_TILE_ID;
   int res = -1;

   for (SInt32 i = 0; i < (SInt32) m_thread_state.size(); i++)
   {
      for (SInt32 j = 0; j < (SInt32) m_thread_state[i].size(); j++)
      {
         if (m_thread_state[i][j].os_tid == os_tid)
         {
            if(thread_idx == INVALID_THREAD_ID)
            {
               tile_id = i;
               thread_idx = j;
            }
            else
               LOG_PRINT_ERROR("Two threads %i on %i and %i on %i have the same os_tid %i!", thread_idx, tile_id, j, i, os_tid);
         }
      }
   }

   if (thread_idx != INVALID_THREAD_ID)
   {
      res = 0;
      getThreadAffinity(tile_id, thread_idx, set);
   }

   return res;
}

void ThreadManager::getThreadAffinity(tile_id_t tile_id, thread_id_t tidx, cpu_set_t* set)
{
   CPU_ZERO_S(CPU_ALLOC_SIZE(Config::getSingleton()->getTotalTiles()), set);
   CPU_OR_S(CPU_ALLOC_SIZE(Config::getSingleton()->getTotalTiles()), set, m_thread_state[tile_id][tidx].cpu_set, set);
}


void ThreadManager::setThreadState(tile_id_t tile_id, thread_id_t tidx, ThreadManager::ThreadState state)
{
   m_thread_state[tile_id][tidx].status = state.status;
   m_thread_state[tile_id][tidx].waiter_core.tile_id = state.waiter_core.tile_id;
   m_thread_state[tile_id][tidx].waiter_core.core_type = state.waiter_core.core_type;
   m_thread_state[tile_id][tidx].waiter_tid = state.waiter_tid;
   m_thread_state[tile_id][tidx].thread_id = state.thread_id;
   m_thread_state[tile_id][tidx].cpu_set = state.cpu_set;
}

SInt32 ThreadManager::getTileIDXFromTileID(tile_id_t tile_id)
{
   SInt32 idx = 0;
   Config::TileList tile_ID_list = config->getTileIDList();
   for (Config::TileList::const_iterator itr = tile_ID_list.begin(); itr != tile_ID_list.end(); itr ++)
   {
      if (tile_id == *itr)
         return idx;
      idx ++;
   }
   LOG_PRINT_ERROR("Tile-ID(%i) not found in Tile-ID-List", tile_id);
   return 0;
}
