#pragma once

#include "common_types.h"

class PerformanceCounterManager
{
public:
   enum MsgType
   {
      ENABLE = 0,
      DISABLE
   };

   PerformanceCounterManager();
   ~PerformanceCounterManager();

   void togglePerformanceCounters(Byte* msg);
};
