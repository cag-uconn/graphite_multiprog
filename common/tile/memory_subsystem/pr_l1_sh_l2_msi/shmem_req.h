#pragma once

#include <vector>
using std::vector;
#include "shmem_msg.h"
#include "common_types.h"
#include "time_types.h"

namespace PrL1ShL2MSI
{

class ShmemReq
{
public:
   ShmemReq(const ShmemMsg& shmem_msg, Time time);
   ~ShmemReq();

   const ShmemMsg& getShmemMsg() const
   { return _shmem_msg; }
   Time getTime() const
   { return _time; }
   void updateTime(Time time);

private:
   const ShmemMsg _shmem_msg;
   Time _time;
};

}
