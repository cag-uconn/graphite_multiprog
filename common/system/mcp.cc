#include <syscall.h>
#include <sched.h>
#include <iostream>

#include "mcp.h"
#include "config.h"
#include "tile_manager.h"
#include "log.h"
#include "tile.h"
#include "simulator.h"
#include "syscall.h"
#include "thread_manager.h"
#include "thread_scheduler.h"

using namespace std;

MCP::MCP(Network& network)
   : _finished(false)
   , _network(network)
   , _MCP_SERVER_MAX_BUFF(256*1024)
   , _scratch(new char[_MCP_SERVER_MAX_BUFF])
   , _vm_manager()
   , _syscall_server(_network, _send_buff, _recv_buff, _MCP_SERVER_MAX_BUFF, _scratch)
   , _sync_server(_network, _recv_buff)
{
   _clock_skew_management_server = ClockSkewManagementServer::create(
                                       Sim()->getCfg()->getString("clock_skew_management/scheme"),
                                       _network, _recv_buff);
   _thread = Thread::create(this);
}

MCP::~MCP()
{
   delete _thread;
   if (_clock_skew_management_server)
      delete _clock_skew_management_server;
   delete [] _scratch;
}

void MCP::processPacket()
{
   _send_buff.clear();
   _recv_buff.clear();

   NetPacket recv_pkt;

   NetMatch match;
   match.types.push_back(MCP_REQUEST_TYPE);
   match.types.push_back(MCP_SYSTEM_TYPE);
   recv_pkt = _network.netRecv(match);

   _recv_buff << make_pair(recv_pkt.data, recv_pkt.length);

   int msg_type;

   _recv_buff >> msg_type;

   LOG_PRINT("MCP message type(%i), sender(%i)", (SInt32) msg_type, recv_pkt.sender.tile_id);

   switch (msg_type)
   {
   case MCP_MESSAGE_SYS_CALL:
      _syscall_server.handleSyscall(recv_pkt.sender);
      break;
   case MCP_MESSAGE_QUIT:
      LOG_PRINT("Quit message received.");
      _finished = true;
      break;

   case MCP_MESSAGE_MUTEX_INIT:
      _sync_server.mutexInit(recv_pkt.sender);
      break;
   case MCP_MESSAGE_MUTEX_LOCK:
      _sync_server.mutexLock(recv_pkt.sender);
      break;
   case MCP_MESSAGE_MUTEX_UNLOCK:
      _sync_server.mutexUnlock(recv_pkt.sender);
      break;

   case MCP_MESSAGE_COND_INIT:
      _sync_server.condInit(recv_pkt.sender);
      break;
   case MCP_MESSAGE_COND_WAIT:
      _sync_server.condWait(recv_pkt.sender);
      break;
   case MCP_MESSAGE_COND_SIGNAL:
      _sync_server.condSignal(recv_pkt.sender);
      break;
   case MCP_MESSAGE_COND_BROADCAST:
      _sync_server.condBroadcast(recv_pkt.sender);
      break;

   case MCP_MESSAGE_BARRIER_INIT:
      _sync_server.barrierInit(recv_pkt.sender);
      break;
   case MCP_MESSAGE_BARRIER_WAIT:
      _sync_server.barrierWait(recv_pkt.sender);
      break;

   case MCP_MESSAGE_THREAD_SPAWN_REQUEST_FROM_REQUESTER:
      Sim()->getThreadManager()->masterSpawnThread((ThreadSpawnRequest*)recv_pkt.data);
      break;
   case MCP_MESSAGE_THREAD_SPAWN_REPLY_FROM_SLAVE:
      Sim()->getThreadManager()->masterSpawnThreadReply((ThreadSpawnRequest*)recv_pkt.data);
      break;
   case MCP_MESSAGE_THREAD_YIELD_REQUEST:
      Sim()->getThreadScheduler()->masterYieldThread((ThreadYieldRequest*)recv_pkt.data);
      break;
   case MCP_MESSAGE_THREAD_MIGRATE_REQUEST_FROM_REQUESTER:
      Sim()->getThreadScheduler()->masterMigrateThread( *(SInt32*)((Byte*)recv_pkt.data+sizeof(msg_type)), 
                                                        *(tile_id_t*)((Byte*)recv_pkt.data+sizeof(msg_type)+sizeof(SInt32)), 
                                                        *(UInt32*)((Byte*)recv_pkt.data+sizeof(msg_type)+sizeof(SInt32)+sizeof(tile_id_t))); 
      break;
   case MCP_MESSAGE_THREAD_SETAFFINITY_REQUEST:
      Sim()->getThreadScheduler()->masterSchedSetAffinity((ThreadAffinityRequest*)recv_pkt.data);
      break;
   case MCP_MESSAGE_THREAD_GETAFFINITY_REQUEST:
      Sim()->getThreadScheduler()->masterSchedGetAffinity((ThreadAffinityRequest*)recv_pkt.data);
      break;

   case MCP_MESSAGE_THREAD_SET_OS_TID:
      Sim()->getThreadManager()->masterSetOSTid(*(tile_id_t*)((Byte*)recv_pkt.data+sizeof(msg_type)), 
                                                *(thread_id_t*)((Byte*)recv_pkt.data+sizeof(msg_type)+sizeof(tile_id_t)), 
                                                *(pid_t*)((Byte*)recv_pkt.data+sizeof(msg_type)+sizeof(tile_id_t)+sizeof(thread_id_t)));
      break;

   case MCP_MESSAGE_THREAD_START:
      Sim()->getThreadManager()->masterOnThreadStart( *(tile_id_t*)((Byte*)recv_pkt.data+sizeof(msg_type)), 
                                                      *(UInt32*)((Byte*)recv_pkt.data+sizeof(msg_type)+sizeof(tile_id_t)), 
                                                      *(SInt32*)((Byte*)recv_pkt.data+sizeof(msg_type)+sizeof(tile_id_t)+sizeof(UInt32)));

      break;
   case MCP_MESSAGE_THREAD_EXIT:
      Sim()->getThreadManager()->masterOnThreadExit(  *(tile_id_t*)((Byte*)recv_pkt.data+sizeof(msg_type)), 
                                                      *(UInt32*)((Byte*)recv_pkt.data+sizeof(msg_type)+sizeof(tile_id_t)), 
                                                      *(SInt32*)((Byte*)recv_pkt.data+sizeof(msg_type)+sizeof(tile_id_t)+sizeof(UInt32)), 
                                                      recv_pkt.time.getTime());
      break;

   case MCP_MESSAGE_THREAD_JOIN_REQUEST:
      Sim()->getThreadManager()->masterJoinThread((ThreadJoinRequest*)recv_pkt.data, recv_pkt.time);
      break;

   case MCP_MESSAGE_CLOCK_SKEW_MANAGEMENT:
      assert(_clock_skew_management_server);
      _clock_skew_management_server->processSyncMsg(recv_pkt.sender);
      break;

   default:
      LOG_PRINT_ERROR("Unhandled MCP message type: %i from %i", msg_type, recv_pkt.sender);
   }

   delete [](Byte*)recv_pkt.data;

   LOG_PRINT("Finished processing message -- type : %d", (int)msg_type);
}

void MCP::spawnThread()
{
   LOG_PRINT("Spawning MCP thread");
   _thread->spawn();
}

void MCP::quitThread()
{
   LOG_PRINT("Send MCP thread quit message");
   SInt32 msg_type = MCP_MESSAGE_QUIT;
   _network.netSend(Config::getSingleton()->getMainMCPCoreId(), MCP_SYSTEM_TYPE, &msg_type, sizeof(msg_type));   //sqc_multi may need to change later
   // Join thread
   _thread->join();
}

void MCP::run()
{
   core_id_t mcp_core_id = Config::getSingleton()->getTargetMCPCoreId();   //sqc_multi may need to change later
   LOG_PRINT("Initial MCP thread in MCP.cc, MCP core id: %d", mcp_core_id);   //sqc_multi
   Sim()->getTileManager()->initializeThread(mcp_core_id);
   Sim()->getTileManager()->initializeCommId(mcp_core_id.tile_id);

   while (!_finished)
      processPacket();
}

