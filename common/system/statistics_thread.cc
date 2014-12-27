#include <cassert>
#include "statistics_thread.h"
#include "statistics_manager.h"
#include "log.h"

StatisticsThread::StatisticsThread(StatisticsManager* manager)
   : _manager(manager)
   , _finished(false)
   , _flag(false)
{
   _thread = Thread::create(this);
}

StatisticsThread::~StatisticsThread()
{
   delete _thread;
}

void
StatisticsThread::run()
{
   LOG_PRINT("Statistics thread starting...");

   while (!_finished)
   {
      _lock.acquire();
      _cond_var.wait(_lock);
      _lock.release();
      
      if (!_finished)
      {
         // Simulation still running
         assert(_flag);
         // Call statistics manager
         _manager->outputPeriodicSummary();
         _flag = false;
      }
   }

   LOG_PRINT("Statistics thread exiting");
}

void
StatisticsThread::spawn()
{
   _thread->spawn();
}

void
StatisticsThread::quit()
{
   _finished = true;
   _cond_var.signal();
   // Wait till the thread exits
   _thread->join();
}

void
StatisticsThread::notify(UInt64 time)
{
   if ((time % _manager->getSamplingInterval()) == 0)
   {
      LOG_ASSERT_WARNING(!_flag, "Sampling interval too small");
      _flag = true;
      _cond_var.signal();
   }
}
