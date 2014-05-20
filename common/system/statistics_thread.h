#pragma once

#include "fixed_types.h"
#include "thread.h"
#include "cond.h"

class StatisticsManager;

class StatisticsThread : public Runnable
{
public:
   StatisticsThread(StatisticsManager* manager);
   ~StatisticsThread();

   void spawn();
   void quit();
   void notify(UInt64 time);

private:
   void run();

   Thread* _thread;
   StatisticsManager* _manager;
   bool _finished;
   ConditionVariable _cond_var;
   // Kind of useless. used just because of the interface of
   // condition variable operations
   Lock _lock;
   bool _flag;
};
