#include "lcp.h"
#include "simulator.h"
#include "tile.h"
#include "message_types.h"
#include "thread_manager.h"
#include "tile_manager.h"
#include "performance_counter_manager.h"
#include "clock_skew_management_object.h"

#include "log.h"

// -- general LCP functionality

LCP::LCP()
   : _finished(false)
   , _proc_num(Config::getSingleton()->getCurrentProcessNum())
   , _transport(Transport::getSingleton()->getGlobalNode())
{
   _thread = Thread::create(this);
}

LCP::~LCP()
{
   delete _thread;
}

void LCP::run()
{
   LOG_PRINT("LCP started.");

   while (!_finished)
      processPacket();
}

void LCP::processPacket()
{
   Byte *pkt = _transport->recv();

   SInt32 *msg_type = (SInt32*)pkt;

   LOG_PRINT("Received message type: %d", *msg_type);

   Byte *data = pkt + sizeof(SInt32);

   switch (*msg_type)
   {
   case LCP_MESSAGE_QUIT:
      LOG_PRINT("Received quit message.");
      _finished = true;
      break;

   case LCP_MESSAGE_COMMID_UPDATE:
      updateCommId(data);
      break;

   case LCP_MESSAGE_SIMULATOR_FINISHED:
      Sim()->handleFinish();
      break;

   case LCP_MESSAGE_SIMULATOR_FINISHED_ACK:
      Sim()->deallocateProcess();
      break;

   case LCP_MESSAGE_THREAD_SPAWN_REQUEST_FROM_MASTER:
      Sim()->getThreadManager()->slaveSpawnThread((ThreadSpawnRequest*)pkt);
      break;
      
   case LCP_MESSAGE_QUIT_THREAD_SPAWNER:
      Sim()->getThreadManager()->slaveTerminateThreadSpawner();
      break;

   case LCP_MESSAGE_QUIT_THREAD_SPAWNER_ACK:
      Sim()->getThreadManager()->updateTerminateThreadSpawner();
      break;

   case LCP_MESSAGE_TOGGLE_PERFORMACE_COUNTERS:
      Sim()->getPerformanceCounterManager()->togglePerformanceCounters(data);
      break;

   case LCP_MESSAGE_CLOCK_SKEW_MANAGEMENT:
      assert (Sim()->getClockSkewManagementManager());
      Sim()->getClockSkewManagementManager()->processSyncMsg(data);
      break;

   default:
      LOG_ASSERT_ERROR(false, "Unexpected message type: %d.", *msg_type);
      break;
   }

   delete [] pkt;
}

void LCP::spawnThread()
{
   _thread->spawn();
}

void LCP::quitThread()
{
   LOG_PRINT("Send LCP quit message");

   SInt32 msg_type = LCP_MESSAGE_QUIT;
   _transport->globalSend(_proc_num,
                           &msg_type,
                           sizeof(msg_type));
   // Join thread
   _thread->join();
}

// -- functions for specific tasks

struct CommMapUpdate
{
   SInt32 comm_id;
   tile_id_t tile_id;
};

void LCP::updateCommId(void *vp)
{
   CommMapUpdate *update = (CommMapUpdate*)vp;

   LOG_PRINT("Initializing comm_id: %d to tile_id: %d", update->comm_id, update->tile_id);
   Config::getSingleton()->updateCommToTileMap(update->comm_id, update->tile_id);

   NetPacket ack(/*time*/ Time(0),
                 /*type*/ LCP_COMM_ID_UPDATE_REPLY,
                 /*sender*/ 0, // doesn't matter ; see tile_manager.cc
                 /*receiver*/ update->tile_id,
                 /*length*/ 0,
                 /*data*/ NULL);
   Byte *buffer = ack.makeBuffer(LCP_HEAP_ID);
   _transport->send(update->tile_id, buffer, ack.bufferSize());
   delete [] buffer;
}
