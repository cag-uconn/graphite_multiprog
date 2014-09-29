#pragma once

#include "directory_entry.h"
#include "random.h"

class DirectoryEntryLimited : virtual public DirectoryEntry
{
public:
   DirectoryEntryLimited(SInt32 max_hw_sharers);
   ~DirectoryEntryLimited();

   // Sharer list query operations 
   bool isSharer(tile_id_t sharer_id) const;
   bool isTrackedSharer(tile_id_t sharer_id) const;
   bool getSharersList(vector<tile_id_t>& sharers) const;
   tile_id_t getOneSharer();
   SInt32 getNumSharers() const;
   
   // Sharer list manipulation operations  
   bool addSharer(tile_id_t sharer_id);
   void removeSharer(tile_id_t sharer_id);

protected:
   vector<SInt16> _sharers;
   SInt32 _num_tracked_sharers;
   static const SInt16 INVALID_SHARER = 0xffff;

private:
   Random<int> _rand_num;
};
