#include "sim_thread.h"
#include "tile_manager.h"
#include "log.h"
#include "simulator.h"
#include "tile.h"
#include "sim_thread_manager.h"

SimThread::SimThread()
{
   _thread = Thread::create(this);
}

SimThread::~SimThread()
{
   delete _thread;
}

void SimThread::run()
{
   LOG_PRINT("Sim thread starting...");
   _tile_id = Sim()->getTileManager()->registerSimThread();

   Network *net = Sim()->getTileManager()->getTileFromID(_tile_id)->getNetwork();
   bool cont = true;

   // Turn off cont when we receive a quit message
   net->registerCallback(SIM_THREAD_TERMINATE_THREADS,
                         terminateFunc,
                         &cont);

   // Actual work gets done here
   while (cont)
      net->netPullFromTransport();

   LOG_PRINT("Sim thread exiting");
}

void SimThread::terminateFunc(void *vp, NetPacket pkt)
{
   bool *pcont = (bool*) vp;
   *pcont = false;
}

void SimThread::start()
{
   _thread->spawn();
}

void SimThread::quit()
{
   NetPacket pkt(Time(0), SIM_THREAD_TERMINATE_THREADS, 0, 0, 0, NULL);
   pkt.receiver = Tile::getMainCoreId(_tile_id);
   
   Transport::Node *global_node = Transport::getSingleton()->getGlobalNode();
   global_node->send(_tile_id, &pkt, pkt.bufferSize());
}
