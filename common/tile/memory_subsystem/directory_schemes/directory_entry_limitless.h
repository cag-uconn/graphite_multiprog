#pragma once

#include "directory_entry_limited.h"
#include "bit_vector.h"

class DirectoryEntryLimitless : public DirectoryEntryLimited
{
public:
   DirectoryEntryLimitless(SInt32 max_hw_sharers, SInt32 max_num_sharers);
   ~DirectoryEntryLimitless();
   
   // Sharer list query operations
   bool isSharer(tile_id_t sharer_id) const;
   bool isTrackedSharer(tile_id_t sharer_id) const;
   bool getSharersList(vector<tile_id_t>& sharers_list) const;
   SInt32 getNumSharers() const;

   // Sharer list manipulation operations
   bool addSharer(tile_id_t sharer_id);
   void removeSharer(tile_id_t sharer_id);

   // Latency for accessing directory entry
   UInt32 getLatency() const;

private:
   // Software Sharers
   BitVector* _software_sharers;

   // Max Num Sharers - For Software Trap
   SInt32 _max_num_sharers;

   // Software Trap Variables
   bool _software_trap_enabled;
   static UInt32 _software_trap_penalty;
   static bool _software_trap_penalty_initialized;
};
