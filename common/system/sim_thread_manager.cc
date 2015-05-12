#include "sim_thread_manager.h"

#include "lock.h"
#include "log.h"
#include "config.h"
#include "simulator.h"
#include "tile_manager.h"
#include "tile.h"
#include "mcp.h"

SimThreadManager::SimThreadManager()
{
}

SimThreadManager::~SimThreadManager()
{
}

void SimThreadManager::spawnThreads()
{
   UInt32 num_sim_threads = Config::getSingleton()->getNumLocalTiles();

   LOG_PRINT("Starting %d threads on proc: %d.",
              num_sim_threads, Config::getSingleton()->getCurrentProcessNum());

   _sim_threads = new SimThread[num_sim_threads];
   for (UInt32 i = 0; i < num_sim_threads; i++)
   {
      LOG_PRINT("Starting thread %i", i);
      _sim_threads[i].start();
   }
}

void SimThreadManager::quitThreads()
{
   LOG_PRINT("Sending quit messages.");

   UInt32 num_sim_threads = Config::getSingleton()->getNumLocalTiles();
   for (UInt32 i = 0; i < num_sim_threads; i++)
   {
      LOG_PRINT("Quiting thread %i", i);
      _sim_threads[i].quit();
   }

   LOG_PRINT("All threads have exited.");
}
