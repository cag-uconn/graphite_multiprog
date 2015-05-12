// config.h
//
// The Config class is used to set, store, and retrieve all the configurable
// parameters of the simulator.
//
// Initial creation: Sep 18, 2008 by jasonm

#ifndef CONFIG_H
#define CONFIG_H

#include <vector>
#include <set>
#include <map>
#include <string>
#include <iostream>
#include <cassert>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "fixed_types.h"

class Config
{
public:
   class TileParameters
   {
   public:
      TileParameters(std::string core_type,
                     std::string l1_icache_type, std::string l1_dcache_type, std::string l2_cache_type)
         : m_core_type(core_type)
         , m_l1_icache_type(l1_icache_type)
         , m_l1_dcache_type(l1_dcache_type)
         , m_l2_cache_type(l2_cache_type)
      {}
      ~TileParameters() {}

      std::string getCoreType() const     { return m_core_type; }
      std::string getL1ICacheType() const { return m_l1_icache_type; }
      std::string getL1DCacheType() const { return m_l1_dcache_type; }
      std::string getL2CacheType() const  { return m_l2_cache_type; }
   
   private:
      std::string m_core_type;
      std::string m_l1_icache_type;
      std::string m_l1_dcache_type;
      std::string m_l2_cache_type;
   };

   class NetworkParameters
   {
   public:
      NetworkParameters(std::string type)
         : m_type(type)
      {}
      ~NetworkParameters() {}

      std::string getType() const         { return m_type; }
   
   private:
      std::string m_type;
   };

   enum SimulationMode
   {
      FULL = 0,
      LITE,
      NUM_SIMULATION_MODES
   };

   typedef std::vector<tile_id_t> TileList;
   typedef std::vector<UInt32> ProcessList;
   typedef std::vector<UInt32> TileToProcMap;
   typedef std::vector<tile_id_t>::const_iterator TLCI;
   typedef std::map<UInt32,tile_id_t> CommToTileMap;

   Config();
   ~Config();

   void loadFromFile(char* filename);
   void loadFromCmdLine();

   // Return the number of processes involved in this simulation
   UInt32 getProcessCount() const               { return m_num_processes; }
   void setProcessCount(UInt32 num_processes)   { m_num_processes = num_processes; }

   // Retrieve and set the process number for this process (I'm expecting
   //  that the initialization routine of the Transport layer will set this)
   UInt32 getCurrentProcessNum() const          { return m_current_process_num; }
   void setProcessNum(UInt32 proc_num)          { m_current_process_num = proc_num; setMasterProcessNum(proc_num); }

   // Num of target processes
   UInt32 getTargetCount() const                { return m_num_targets; }
   void setTargetCount(UInt32 num_targets)      { m_num_targets = num_targets; }
   
   // Target process ID
   UInt32 getCurrentTargetNum() const           { return m_current_target_num; }
   void setTargetNum(UInt32 target_num)         { m_current_target_num = target_num; }
   
   // Number of processes for this target
   UInt32 getProcessCountCurrentTarget() const { return m_num_processes_current_target; }
   void setProcessCountCurrentTarget(UInt32 process_count) { m_num_processes_current_target = process_count; }
   
   // Check whether this process is the main process of this target  //sqc_multi
   bool isMasterProcess() const                        { return m_current_process_num == m_master_process_num; }
   UInt32 getMasterProcessNum() const                  { return m_master_process_num; }
   void setMasterProcessNum(UInt32 master_process_num) { m_master_process_num = master_process_num; }
 
   // Process num list for current target
   ProcessList getProcessNumList() const
   { return ProcessList(1, getCurrentProcessNum()); }

   // Get master thread tile ID
   tile_id_t getMasterThreadTileIDForTarget(UInt32 target_id) const
   { return target_id; }
   tile_id_t getMasterThreadTileID() const
   { return getCurrentProcessNum(); }

   // Get master process ID from target ID
   UInt32 getMasterProcessID(UInt32 target_id)
   { return target_id; }

   // Get MCP tile/core ID
   tile_id_t getMasterMCPTileID () const  { return (getTotalTiles() - m_num_targets); }
   core_id_t getMasterMCPCoreID() const   { return (core_id_t) {(tile_id_t) (getTotalTiles() - m_num_targets), MAIN_CORE_TYPE}; }
   tile_id_t getMCPTileID() const         { return (getTotalTiles() - m_num_targets + m_current_target_num); }
   core_id_t getMCPCoreID() const         { return (core_id_t) {(tile_id_t) (getTotalTiles() - m_num_targets + m_current_target_num), MAIN_CORE_TYPE}; }
  
   // Get Thread Spawner tile/core IDs 
   TileList getThreadSpawnerTileIDList() const;
   tile_id_t getThreadSpawnerTileID(UInt32 proc_num) const;
   core_id_t getThreadSpawnerCoreID(UInt32 proc_num) const;
   tile_id_t getCurrentThreadSpawnerTileID() const;
   core_id_t getCurrentThreadSpawnerCoreID() const;

   // Return the number of modules (tiles) in a given process
   UInt32 getNumTilesInProcess(UInt32 proc_num) const
   {
      assert (proc_num < m_num_processes); 
      return m_proc_to_tile_list_map[proc_num].size(); 
   }

   SInt32 getIndexFromTileID(UInt32 proc_num, tile_id_t tile_id);
   tile_id_t getTileIDFromIndex(UInt32 proc_num, SInt32 index);
   core_id_t getMainCoreIDFromIndex(UInt32 proc_num, SInt32 index);
   
   UInt32 getNumLocalTiles() const     { return getNumTilesInProcess(getCurrentProcessNum()); }
   UInt32 getMaxThreadsPerCore() const { return m_max_threads_per_core;}
   UInt32 getNumCoresPerTile() const   { return m_num_cores_per_tile;}

   // Return the number of tiles in all applications
   UInt32 getTotalTiles() const;
   UInt32 getApplicationTiles() const;
   // Returns the number of tiles in only current application processes
   UInt32 getTotalTilesCurrentTarget() const       { return m_total_tiles_current_target; }
   UInt32 getApplicationTilesCurrentTarget() const { return m_application_tiles_current_target; }
   bool isApplicationTile(tile_id_t tile_id) const;

   // Return an array of tile numbers for a given process
   //  The returned array will have numMods(proc_num) elements
   const TileList & getTileListForProcess(UInt32 proc_num) const
   { assert(proc_num < m_num_processes); return m_proc_to_tile_list_map[proc_num]; }
   const TileList & getApplicationTileListForProcess(UInt32 proc_num) const
   { assert(proc_num < m_num_processes); return m_proc_to_application_tile_list_map[proc_num]; }

   const TileList & getTileListForCurrentProcess() const
   { return getTileListForProcess(getCurrentProcessNum()); }

   UInt32 getProcessNumForTile(tile_id_t tile_id) const
   {
     LOG_ASSERT_ERROR (tile_id < (tile_id_t) m_total_tiles, "Tile-ID:%i, Total-Tiles:%u", tile_id, m_total_tiles);
     return m_tile_to_proc_map[tile_id]; 
   }

   // Get tile ID list for target
   const TileList getTileIDList() const;

   // For mapping between user-land communication id's to actual tile id's
   void updateCommToTileMap(UInt32 comm_id, tile_id_t tile_id);
   UInt32 getTileFromCommId(UInt32 comm_id);

   // Get Tile ID length (in bits)
   UInt32 getTileIDLength() const
   { return m_tile_id_length; }

   SimulationMode getSimulationMode() const
   { return m_simulation_mode; }

   // Tile & Network Parameters
   std::string getCoreType(tile_id_t tile_id) const;
   std::string getL1ICacheType(tile_id_t tile_id) const;
   std::string getL1DCacheType(tile_id_t tile_id) const;
   std::string getL2CacheType(tile_id_t tile_id) const;

   std::string getNetworkType(SInt32 network_id) const;

   // Knobs
   bool isSimulatingSharedMemory() const;
   bool getEnableCoreModeling() const;
   bool getEnablePowerModeling() const;
   bool getEnableAreaModeling() const;

   // Generate mapping of tile to processes
   void generateTileMap();
   
   // Logging
   std::string getOutputFileName() const;
   std::string formatOutputFileName(std::string filename) const;

   static Config *getSingleton();

private:
   std::vector<TileList> computeProcessToTileMapping();
   
   UInt32  m_num_processes;         // Total number of processes (incl myself)
   UInt32  m_num_targets;           // Total number of targets (incl myself)  //sqc_multi
   UInt32  m_total_tiles;           // Total number of tiles in all processes
   UInt32  m_application_tiles;     // Total number of tiles used by applications
   UInt32  m_num_cores_per_tile;    // Number of cores per tile
   UInt32  m_tile_id_length;        // Number of bits needed to store a tile_id
   UInt32  m_max_threads_per_core;

   UInt32  m_current_process_num;   // Process number for this process
   UInt32  m_current_target_num;    // Target number for this process
   UInt32  m_master_process_num;    // Master process number for this target 
   UInt32  m_num_processes_current_target;  // Number of host processes in this target 
   UInt32  m_application_tiles_current_target; // Number of application tiles used in this target 
   UInt32  m_total_tiles_current_target; // Number of total tiles used in this target 

   std::vector<TileParameters> m_tile_parameters_vec;         // Vector holding main tile parameters
   std::vector<NetworkParameters> m_network_parameters_vec;   // Vector holding network parameters

   // This data structure keeps track of which tiles are in each process.
   // It is an array of size num_processes where each element is a list of
   // tile numbers.  Each list specifies which tiles are in the corresponding
   // process.

   TileToProcMap m_tile_to_proc_map;
   TileList* m_proc_to_tile_list_map;
   TileList* m_proc_to_application_tile_list_map;
   CommToTileMap m_comm_to_tile_map;

   // Simulation Mode
   SimulationMode m_simulation_mode;

   UInt32  m_mcp_process;          // The process where the MCP lives

   ProcessList m_process_list_current_target;  // The process indexes of current target

   static Config *m_singleton;

   static UInt32 m_knob_total_tiles;
   static UInt32 m_knob_max_threads_per_core;
   static UInt32 m_knob_num_process;
   static UInt32 m_knob_num_target;  
   static bool m_knob_simarch_has_shared_mem;
   static std::string m_knob_output_file;
   static bool m_knob_enable_core_modeling;
   static bool m_knob_enable_power_modeling;
   static bool m_knob_enable_area_modeling;
   static char* m_knob_proc_index_str;
   static char* m_knob_target_index_str; 

   // Get Tile & Network Parameters
   void parseTileParameters();
   void parseNetworkParameters();

   static SimulationMode parseSimulationMode(std::string mode);
   static UInt32 computeTileIDLength(UInt32 tile_count);
   static bool isTileCountPermissible(UInt32 tile_count);
   
   void logTileMap();
};

#endif
