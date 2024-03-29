#pragma once

#include <string>
using std::string;

// Forward declaration
namespace PrL1PrL2DramDirectoryMSI
{
   class L2CacheCntlr;
   class MemoryManager;
}

#include "tile.h"
#include "cache.h"
#include "shmem_msg.h"
#include "mem_component.h"
#include "fixed_types.h"
#include "shmem_perf_model.h"
#include "cache_replacement_policy.h"
#include "cache_hash_fn.h"

namespace PrL1PrL2DramDirectoryMSI
{
   class L1CacheCntlr
   {
   public:
      L1CacheCntlr(MemoryManager* memory_manager,
                   UInt32 cache_line_size,
                   UInt32 l1_icache_size,
                   UInt32 l1_icache_associativity,
                   UInt32 l1_icache_num_banks,
                   string l1_icache_replacement_policy,
                   UInt32 l1_icache_data_access_cycles,
                   UInt32 l1_icache_tags_access_cycles,
                   string l1_icache_perf_model_type,
                   bool l1_icache_track_miss_types,
                   UInt32 l1_dcache_size,
                   UInt32 l1_dcache_associativity,
                   UInt32 l1_dcache_num_banks,
                   string l1_dcache_replacement_policy,
                   UInt32 l1_dcache_data_access_cycles,
                   UInt32 l1_dcache_tags_access_cycles,
                   string l1_dcache_perf_model_type,
                   bool l1_dcache_track_miss_types);
      ~L1CacheCntlr();

      Cache* getL1ICache() { return _l1_icache; }
      Cache* getL1DCache() { return _l1_dcache; }

      void setL2CacheCntlr(L2CacheCntlr* l2_cache_cntlr);

      bool processMemOpFromCore(MemComponent::Type mem_component,
                                Core::lock_signal_t lock_signal,
                                Core::mem_op_t mem_op_type, 
                                IntPtr address, UInt32 offset,
                                Byte* data_buf, UInt32 data_length);

      void insertCacheLine(MemComponent::Type mem_component,
                           IntPtr address, CacheState::Type cstate, const Byte* fill_buf,
                           bool* eviction, IntPtr* evicted_address);

      CacheState::Type getCacheLineState(MemComponent::Type mem_component, IntPtr address);
      void setCacheLineState(MemComponent::Type mem_component, IntPtr address, CacheState::Type cstate);
      void invalidateCacheLine(MemComponent::Type mem_component, IntPtr address);

      void addSynchronizationCost(MemComponent::Type mem_component, module_t module);

   private:
      MemoryManager* _memory_manager;
      Cache* _l1_icache;
      Cache* _l1_dcache;
      CacheReplacementPolicy* _l1_icache_replacement_policy_obj;
      CacheReplacementPolicy* _l1_dcache_replacement_policy_obj;
      CacheHashFn* _l1_icache_hash_fn_obj;
      CacheHashFn* _l1_dcache_hash_fn_obj;
      L2CacheCntlr* _l2_cache_cntlr;

      void accessCache(MemComponent::Type mem_component,
            Core::mem_op_t mem_op_type, 
            IntPtr address, UInt32 offset,
            Byte* data_buf, UInt32 data_length);
      bool operationPermissibleinL1Cache(MemComponent::Type mem_component,
            IntPtr address, Core::mem_op_t mem_op_type,
            UInt32 access_num);

      Cache* getL1Cache(MemComponent::Type mem_component);
      ShmemMsg::Type getShmemMsgType(Core::mem_op_t mem_op_type);

      // Utilities
      tile_id_t getTileID();
      UInt32 getCacheLineSize();
      ShmemPerfModel* getShmemPerfModel();
   };
}
