#pragma once

#include "directory_entry.h"
#include "bit_vector.h"
#include "random.h"

class DirectoryEntryFullMap : virtual public DirectoryEntry
{
public:
   DirectoryEntryFullMap(SInt32 max_hw_sharers);
   ~DirectoryEntryFullMap();
  
   // Sharer list query operations 
   bool isSharer(tile_id_t sharer_id) const;
   bool isTrackedSharer(tile_id_t sharer_id) const;
   bool getSharersList(vector<tile_id_t>& sharers_list) const;
   tile_id_t getOneSharer();
   SInt32 getNumSharers() const;

   // Sharer list manipulation operations
   bool addSharer(tile_id_t sharer_id);
   void removeSharer(tile_id_t sharer_id);

private:
   BitVector* _sharers;
   Random<int> _rand_num;
};
