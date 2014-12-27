#ifndef SIM_THREAD_MANAGER_H
#define SIM_THREAD_MANAGER_H

#include "sim_thread.h"

class SimThreadManager
{
public:
   SimThreadManager();
   ~SimThreadManager();

   void spawnThreads();
   void quitThreads();

private:
   SimThread *_sim_threads;
};

#endif // SIM_THREAD_MANAGER
