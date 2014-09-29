#pragma once

#include <string>
#include <vector>
using std::string;
using std::vector;

// Forward Decls
class DirectoryEntry;
class DirectoryEntryFactory;

#include "fixed_types.h"
#include "directory_type.h"
#include "caching_protocol.h"

class Directory
{
public:
   Directory(DirectoryEntryFactory* factory, UInt32 directory_type, SInt32 total_entries);
   ~Directory();

   DirectoryEntry* getDirectoryEntry(SInt32 entry_num);
   void setDirectoryEntry(SInt32 entry_num, DirectoryEntry* directory_entry);
   
   // Sharer Stats
   void updateSharerStats(SInt32 old_sharer_count, SInt32 new_sharer_count);
   void getSharerStats(vector<UInt64>& sharer_count_vec);

private:
   SInt32 _total_entries;
   UInt32 _directory_type;

   vector<DirectoryEntry*> _directory_entry_list;
   vector<UInt64> _sharer_count_vec;
   
   void initializeSharerStats();
};