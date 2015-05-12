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

   // Called by MCP
   void masterTogglePerformanceCountersRequest(Byte* msg, core_id_t core_id);
   void masterTogglePerformanceCountersResponse();
   // Called by LCP
   void togglePerformanceCounters(Byte* msg);

private:
   UInt32 _num_toggle_requests_received;
   UInt32 _num_toggle_responses_received;
};
