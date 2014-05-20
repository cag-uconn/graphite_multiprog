#pragma once

#include <string>
using std::string;

#include "statistics_thread.h"
#include "fixed_types.h"

class StatisticsManager
{
public:
   enum StatisticType
   {
      CACHE_LINE_REPLICATION = 0,
      NETWORK_UTILIZATION,
      NUM_STATISTIC_TYPES
   };

   StatisticsManager();
   ~StatisticsManager();
   void outputPeriodicSummary();
   UInt64 getSamplingInterval() const { return _sampling_interval; }

   StatisticsThread* getThread() const    { return _thread; }
   void spawnThread()   {  _thread->spawn();  }
   void quitThread()    {  _thread->quit();   }

private:
   bool _statistic_enabled[NUM_STATISTIC_TYPES];
   UInt64 _sampling_interval;

   StatisticsThread* _thread;

   void openTraceFiles();
   void closeTraceFiles();
   StatisticType parseType(string type);
};
