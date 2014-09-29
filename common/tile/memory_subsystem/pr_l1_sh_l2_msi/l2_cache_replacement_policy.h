#pragma once

#include "../cache/cache_replacement_policy.h"
#include "directory_req_queue.h"
#include "shmem_req.h"

namespace PrL1ShL2MSI
{

class L2CacheReplacementPolicy : public CacheReplacementPolicy
{
public:
   L2CacheReplacementPolicy(UInt32 cache_size, UInt32 associativity, UInt32 cache_line_size,
                            DirectoryReqQueue& L2_cache_req_queue);
   ~L2CacheReplacementPolicy();

   UInt32 getReplacementWay(CacheLineInfo** cache_line_info_array, UInt32 set_num);
   void update(CacheLineInfo** cache_line_info_array, UInt32 set_num, UInt32 accessed_way);

private:
   DirectoryReqQueue& _L2_cache_req_queue;
   UInt32 _log_cache_line_size;
   
   IntPtr getAddressFromTag(IntPtr tag) const;
};

}
