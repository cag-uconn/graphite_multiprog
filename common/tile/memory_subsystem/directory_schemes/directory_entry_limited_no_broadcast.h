#pragma once

#include "directory_entry_limited.h"

class DirectoryEntryLimitedNoBroadcast : public DirectoryEntryLimited
{
public:
   DirectoryEntryLimitedNoBroadcast(SInt32 max_hw_sharers)
      : DirectoryEntry(max_hw_sharers)
      , DirectoryEntryLimited(max_hw_sharers)
   {}
   ~DirectoryEntryLimitedNoBroadcast()
   {}
};
