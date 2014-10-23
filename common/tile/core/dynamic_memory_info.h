#pragma once

#include "time_types.h"

class DynamicMemoryInfo
{
public:
   DynamicMemoryInfo(IntPtr address, UInt32 size, Core::mem_op_t mem_op_type, Core::lock_signal_t lock_signal)
      : _address(address)
      , _size(size)
      , _mem_op_type(mem_op_type)
      , _lock_signal(lock_signal)
      , _latency(0)
   {}
   
   IntPtr _address;
   UInt32 _size;
   Core::mem_op_t _mem_op_type;
   Core::lock_signal_t _lock_signal;
   Time _latency;
};

