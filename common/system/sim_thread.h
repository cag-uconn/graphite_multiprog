#ifndef SIM_THREAD_H
#define SIM_THREAD_H

#include "thread.h"
#include "fixed_types.h"
#include "network.h"

class SimThread : public Runnable
{
public:
   SimThread();
   ~SimThread();

   void start();
   void quit();

private:
   void run();

   static void terminateFunc(void *vp, NetPacket pkt);

   Thread* _thread;
   tile_id_t _tile_id;
};

#endif // SIM_THREAD_H
