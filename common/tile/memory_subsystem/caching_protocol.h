#pragma once

#include <string>
using std::string;

#include "log.h"

class CachingProtocol
{
public:
   enum Type
   {
      PR_L1_PR_L2_DRAM_DIRECTORY_MSI = 0,
      PR_L1_PR_L2_DRAM_DIRECTORY_MOSI,
      PR_L1_SH_L2_MSI,
      VICTIM_REPLICATION,
      ADAPTIVE_SELECTIVE_REPLICATION,
      LOCALITY_AWARE_PROTOCOL,
      NUM_TYPES
   };

   static Type parse(string protocol_type)
   {
      if (protocol_type == "pr_l1_pr_l2_dram_directory_msi")
         return PR_L1_PR_L2_DRAM_DIRECTORY_MSI;
      else if (protocol_type == "pr_l1_pr_l2_dram_directory_mosi")
         return PR_L1_PR_L2_DRAM_DIRECTORY_MOSI;
      else if (protocol_type == "pr_l1_sh_l2_msi")
         return PR_L1_SH_L2_MSI;
      else if (protocol_type == "victim_replication")
         return VICTIM_REPLICATION;
      else if (protocol_type == "adaptive_selective_replication")
         return ADAPTIVE_SELECTIVE_REPLICATION;
      else if (protocol_type == "locality_aware_protocol")
         return LOCALITY_AWARE_PROTOCOL;
      else
      {
         LOG_PRINT_ERROR("Unrecognized caching protocol type(%s)", protocol_type.c_str());
         return NUM_TYPES;
      }
   }
};
