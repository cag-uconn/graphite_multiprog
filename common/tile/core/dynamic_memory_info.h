#pragma once

#include "time_types.h"

class DynamicMemoryInfo
{
public:
   DynamicMemoryInfo(IntPtr address, bool read)
      : _address(address)
      , _read(read)
      , _latency(0)
   {}
   
   IntPtr _address;
   bool _read;
   Time _latency;
};

