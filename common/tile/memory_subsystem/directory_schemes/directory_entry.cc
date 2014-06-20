#include "directory_type.h"
#include "directory_entry.h"
#include "directory_entry_full_map.h"
#include "directory_entry_limited_no_broadcast.h"
#include "directory_entry_ackwise.h"
#include "directory_entry_limitless.h"
#include "utils.h"
#include "log.h"

DirectoryEntry::DirectoryEntry(SInt32 max_hw_sharers)
   : _address(INVALID_ADDRESS)
   , _dstate(DirectoryState::UNCACHED)
   , _owner_id(INVALID_TILE_ID)
   , _max_hw_sharers(max_hw_sharers)
{}

DirectoryEntry::~DirectoryEntry()
{}

UInt32
DirectoryEntry::parseDirectoryType(string directory_type)
{
   if (directory_type == "full_map")
      return FULL_MAP;
   else if (directory_type == "limited_no_broadcast")
      return LIMITED_NO_BROADCAST;
   else if (directory_type == "ackwise")
      return ACKWISE;
   else if (directory_type == "limitless")
      return LIMITLESS;
   else
      LOG_PRINT_ERROR("Unsupported Directory Type: %s", directory_type.c_str());
   return UINT32_MAX_;
}

UInt32
DirectoryEntry::getSize(UInt32 directory_type, SInt32 max_hw_sharers, SInt32 max_num_sharers)
{
   LOG_PRINT("DirectoryEntry::getSize(), Directory Type(%u), Max Num Sharers(%i), Max HW Sharers(%i)",
             directory_type, max_num_sharers, max_hw_sharers);
   switch(directory_type)
   {
   case FULL_MAP:
      return max_num_sharers;
   case LIMITED_NO_BROADCAST:
   case ACKWISE:
   case LIMITLESS:
      return max_hw_sharers * ceilLog2(max_num_sharers);
   default:
      LOG_PRINT_ERROR("Unrecognized directory type(%u)", directory_type);
      return 0;
   }
}

void
DirectoryEntry::setOwner(tile_id_t owner_id)
{
   if (owner_id != INVALID_TILE_ID)
      LOG_ASSERT_ERROR(isTrackedSharer(owner_id), "Owner Id(%i) not a sharer, State(%s)",
                       owner_id, SPELL_DSTATE(_dstate));
   _owner_id = owner_id;
}

// DirectoryEntry Factory
DirectoryEntryFactory::DirectoryEntryFactory(tile_id_t tile_id, UInt32 directory_type,
                                             SInt32 max_hw_sharers, SInt32 max_num_sharers)
   : _tile_id(tile_id)
   , _directory_type(directory_type)
   , _max_hw_sharers(max_hw_sharers)
   , _max_num_sharers(max_num_sharers)
{}

DirectoryEntryFactory::~DirectoryEntryFactory()
{}

DirectoryEntry*
DirectoryEntryFactory::create()
{
   switch (_directory_type)
   {
   case FULL_MAP:
      return new(_tile_id) DirectoryEntryFullMap(_max_num_sharers);

   case LIMITED_NO_BROADCAST:
      return new(_tile_id) DirectoryEntryLimitedNoBroadcast(_max_hw_sharers);

   case ACKWISE:
      return new(_tile_id) DirectoryEntryAckwise(_max_hw_sharers);

   case LIMITLESS:
      return new(_tile_id) DirectoryEntryLimitless(_max_hw_sharers, _max_num_sharers);

   default:
      LOG_PRINT_ERROR("Unrecognized Directory Type: %u", _directory_type);
      return NULL;
   }
}

void
DirectoryEntryFactory::destroy(DirectoryEntry* entry)
{
   delete entry;
}
