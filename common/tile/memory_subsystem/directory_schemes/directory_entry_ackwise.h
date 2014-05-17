#pragma once

#include "directory_entry_limited.h"

class DirectoryEntryAckwise : public DirectoryEntryLimited
{
public:
   DirectoryEntryAckwise(SInt32 max_hw_sharers);
   ~DirectoryEntryAckwise();

   // Sharer list query operations 
   bool isSharer(tile_id_t sharer_id) const;
   bool getSharersList(vector<tile_id_t>& sharers_list) const;
   SInt32 getNumSharers() const;
   bool inBroadcastMode() const;
   
   // Sharer list manipulation operations  
   bool addSharer(tile_id_t sharer_id); 
   void removeSharer(tile_id_t sharer_id);

private:
   bool _global_enabled;
   SInt32 _num_untracked_sharers;
};
