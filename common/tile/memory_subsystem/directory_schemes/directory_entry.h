#pragma once

#include <vector>
#include <string>
using std::vector;
using std::string;

#include "common_types.h"
#include "directory_state.h"
#include "directory_type.h"
#include "caching_protocol.h"

class DirectoryEntry
{
public:
   DirectoryEntry(SInt32 max_hw_sharers);
   virtual ~DirectoryEntry();

   static DirectoryEntry* create(UInt32 directory_type, SInt32 max_hw_sharers, SInt32 max_num_sharers);
   static UInt32 getSize(UInt32 directory_type, SInt32 max_hw_sharers, SInt32 max_num_sharers);
   static UInt32 parseDirectoryType(string directory_type);

   // Sharer list query operations
   virtual bool isSharer(tile_id_t sharer_id) const = 0;
   virtual bool isTrackedSharer(tile_id_t sharer_id) const = 0;
   virtual bool getSharersList(vector<tile_id_t>& sharers_list) const = 0;
   virtual tile_id_t getOneSharer() = 0;
   virtual SInt32 getNumSharers() const = 0;
   
   virtual bool inBroadcastMode() const         { return false; }
   
   // Sharer list manipulation operations
   virtual bool addSharer(tile_id_t sharer_id) = 0;
   virtual void removeSharer(tile_id_t sharer_id) = 0;

   // Owner
   tile_id_t getOwner() const                   { return _owner_id; }
   void setOwner(tile_id_t owner_id);

   // Address
   IntPtr getAddress() const                    { return _address; }
   void setAddress(IntPtr address)              { _address = address; }

   // Directory state
   DirectoryState::Type getDState() const       { return _dstate; }
   void setDState(DirectoryState::Type dstate)  { _dstate = dstate; }

   // Latency for accessing directory entry
   virtual UInt32 getLatency() const            { return 0; }

protected:
   IntPtr _address;
   DirectoryState::Type _dstate;
   tile_id_t _owner_id;
   SInt32 _max_hw_sharers;
};
