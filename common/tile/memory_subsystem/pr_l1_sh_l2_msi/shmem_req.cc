#include "shmem_req.h"
#include "utils.h"
#include "log.h"

namespace PrL1ShL2MSI
{

ShmemReq::ShmemReq(const ShmemMsg& shmem_msg, Time time)
   : _shmem_msg(shmem_msg)
   , _time(time)
{
   LOG_ASSERT_ERROR(_shmem_msg.getDataBuf() == NULL, "Shmem Reqs should not have data payloads");
}

ShmemReq::~ShmemReq()
{}

void
ShmemReq::updateTime(Time time)
{
   if (_time < time)
      _time = time;
}

}
