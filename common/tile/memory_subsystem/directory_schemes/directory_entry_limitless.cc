#include "directory_entry_limitless.h"
#include "simulator.h"
#include "config.h"
#include "log.h"

bool DirectoryEntryLimitless::_software_trap_penalty_initialized = false;
UInt32 DirectoryEntryLimitless::_software_trap_penalty;

DirectoryEntryLimitless::DirectoryEntryLimitless(SInt32 max_hw_sharers, SInt32 max_num_sharers)
   : DirectoryEntry(max_hw_sharers)
   , DirectoryEntryLimited(max_hw_sharers)
   , _software_sharers(NULL)
   , _max_num_sharers(max_num_sharers)
   , _software_trap_enabled(false)
{
   if (!_software_trap_penalty_initialized)
   {
      try
      {
         _software_trap_penalty = Sim()->getCfg()->getInt("limitless/software_trap_penalty");
      }
      catch (...)
      {
         LOG_PRINT_ERROR("Could not read [limitless/software_trap_penalty] from the cfg file");
      }
      _software_trap_penalty_initialized = true;
   }
}

DirectoryEntryLimitless::~DirectoryEntryLimitless()
{
   if (_software_sharers)
      delete _software_sharers;
}

bool
DirectoryEntryLimitless::isSharer(tile_id_t sharer_id) const
{
   return isTrackedSharer(sharer_id);
}

bool
DirectoryEntryLimitless::isTrackedSharer(tile_id_t sharer_id) const
{
   if (_software_trap_enabled) // Explicit software tracking of sharers
   {
      return _software_sharers->at(sharer_id);
   }
   else // (!_software_trap_enabled) - Explicit hardware tracking of sharers
   {
      return DirectoryEntryLimited::isTrackedSharer(sharer_id);
   }
}

// Return value says whether the sharer was successfully added
//              'True' if it was successfully added
//              'False' if there will be an eviction before adding
bool
DirectoryEntryLimitless::addSharer(tile_id_t sharer_id)
{
   if (_software_trap_enabled) // Explicit software tracking of sharers
   {
      assert(!_software_sharers->at(sharer_id));
      _software_sharers->set(sharer_id);
   }

   else // (!_software_trap_enabled) - Explicit hardware tracking of sharers
   {
      bool added = DirectoryEntryLimited::addSharer(sharer_id);
      if (!added) // Can't track anymore sharers in hardware
      {
         // Migrate the sharers from hardware to software
         _software_trap_enabled = true;
         _software_sharers = new BitVector(_max_num_sharers);
         for (SInt32 i = 0; i < _max_hw_sharers; i++)
         {
            if (_sharers[i] != INVALID_SHARER)
            {
               assert(_sharers[i] >= 0 && _sharers[i] < _max_num_sharers);
               _software_sharers->set(_sharers[i]);
               _sharers[i] = INVALID_SHARER;
               _num_tracked_sharers --;
            }
         }
         // Add current sharer
         _software_sharers->set(sharer_id);
         LOG_ASSERT_ERROR(_num_tracked_sharers == 0, "Num Tracked Sharers(%i)", _num_tracked_sharers);
      }
   }

   // Always can add a sharer
   return true;
}

void
DirectoryEntryLimitless::removeSharer(tile_id_t sharer_id)
{
   if (_software_trap_enabled) // Explicit software tracking of sharers
   {
      assert(_software_sharers->at(sharer_id));
      _software_sharers->clear(sharer_id);
   }
   else // (!_software_trap_enabled) - Explicit hardware tracking of sharers
   {
      DirectoryEntryLimited::removeSharer(sharer_id);
   }
}

// Return a pair:
// val.first :- 'True' if all tiles are sharers
//              'False' if NOT all tiles are sharers
// val.second :- A list of tracked sharers
bool
DirectoryEntryLimitless::getSharersList(vector<tile_id_t>& sharers_list) const
{
   if (_software_trap_enabled) // Explicit software tracking of sharers
   {
      sharers_list.resize(_software_sharers->size());
      
      _software_sharers->resetFind();

      tile_id_t new_sharer = -1;
      SInt32 i = 0;
      while ((new_sharer = _software_sharers->find()) != -1)
      {
         sharers_list[i] = new_sharer;
         i++;
         assert (i <= (tile_id_t) _software_sharers->size());
      }
   }
   else // (!_software_trap_enabled) - Explicit hardware tracking of sharers
   {
      DirectoryEntryLimited::getSharersList(sharers_list);
   }

   return false;
}

SInt32
DirectoryEntryLimitless::getNumSharers() const
{
   return (_software_trap_enabled) ? _software_sharers->size() : _num_tracked_sharers;
}

UInt32
DirectoryEntryLimitless::getLatency() const
{
   return (_software_trap_enabled) ? _software_trap_penalty : 0;
}
