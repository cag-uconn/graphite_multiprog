#pragma once

#include "time_types.h"

class DynamicBranchInfo
{
public:
   DynamicBranchInfo(bool taken, IntPtr target)
      : _taken(taken)
      , _target(target)
   {}
   
   bool _taken;
   IntPtr _target;
};
