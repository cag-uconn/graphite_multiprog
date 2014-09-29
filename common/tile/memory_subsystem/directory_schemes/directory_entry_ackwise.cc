#include "directory_entry_ackwise.h"
#include "log.h"

DirectoryEntryAckwise::DirectoryEntryAckwise(SInt32 max_hw_sharers)
   : DirectoryEntry(max_hw_sharers)
   , DirectoryEntryLimited(max_hw_sharers)
   , _global_enabled(false)
   , _num_untracked_sharers(0)
{}

DirectoryEntryAckwise::~DirectoryEntryAckwise()
{}

bool
DirectoryEntryAckwise::isSharer(tile_id_t sharer_id) const
{
   return (_global_enabled) ? true : isTrackedSharer(sharer_id);
}

// Returns: Says whether the sharer was successfully added
//              'True' if it was successfully added
//              'False' if there will be an eviction before adding
bool
DirectoryEntryAckwise::addSharer(tile_id_t sharer_id)
{
   if (_global_enabled) // All sharers are NOT explicitly tracked
   {
      _num_untracked_sharers ++;
   }
   else  // All sharers are explicitly tracked
   {
      bool added = DirectoryEntryLimited::addSharer(sharer_id);
      if (!added) // Reached the limit on hardware sharers
      {
         LOG_ASSERT_ERROR((_num_untracked_sharers == 0) && (!_global_enabled) && (_num_tracked_sharers == _max_hw_sharers),
                          "Num Untracked Sharers(%i), Global Enabled(%s), Num Tracked Sharers(%i), Max HW Sharers(%i)",
                          _num_untracked_sharers, _global_enabled ? "true" : "false", _num_tracked_sharers, _max_hw_sharers);
         _global_enabled = true;
         _num_untracked_sharers = 1;
      }
   }

   return true;
}

void
DirectoryEntryAckwise::removeSharer(tile_id_t sharer_id)
{
   if (_global_enabled) // (This sharer_id may or may not be explicitly tracked)
   {
      LOG_ASSERT_ERROR(_num_untracked_sharers > 0, "Num Untracked Sharers(%i)", _num_untracked_sharers);

      if (isTrackedSharer(sharer_id))   // Explicitly tracked
      {
         DirectoryEntryLimited::removeSharer(sharer_id);
      }
      else // NOT explicitly tracked
      {
         _num_untracked_sharers --;
         if (_num_untracked_sharers == 0)
            _global_enabled = false;
      }
   }
   else // (This sharer_id is explicitly tracked)
   {
      DirectoryEntryLimited::removeSharer(sharer_id);
   }
}

bool
DirectoryEntryAckwise::inBroadcastMode() const
{
   return _global_enabled;
}

// Return a pair:
// val.first :- 'True' if all tiles are sharers
//              'False' if NOT all tiles are sharers
// val.second :- A list of tracked sharers
bool
DirectoryEntryAckwise::getSharersList(vector<tile_id_t>& sharers_list) const
{
   DirectoryEntryLimited::getSharersList(sharers_list);
   return _global_enabled;
}

SInt32
DirectoryEntryAckwise::getNumSharers() const
{
   return _num_tracked_sharers + _num_untracked_sharers;
}
