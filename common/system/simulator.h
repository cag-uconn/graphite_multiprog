#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "config.h"
#include "log.h"
#include "config.hpp"

class MCP;
class LCP;
class Transport;
class TileManager;
class Thread;
class ThreadManager;
class ThreadScheduler;
class PerformanceCounterManager;
class SimThreadManager;
class ClockSkewManagementManager;
class StatisticsManager;
class StatisticsThread;
class Network;

class Simulator
{
public:
   Simulator();
   ~Simulator();

   void start();

   static Simulator* getSingleton();
   static void setConfig(config::Config * cfg);
   static void allocate();
   static void release();

   TileManager *getTileManager()                               { return _tile_manager; }
   SimThreadManager *getSimThreadManager()                     { return _sim_thread_manager; }
   ThreadManager *getThreadManager()                           { return _thread_manager; }
   ThreadScheduler *getThreadScheduler()                       { return _thread_scheduler; }
   PerformanceCounterManager *getPerformanceCounterManager()   { return _performance_counter_manager; }
   ClockSkewManagementManager *getClockSkewManagementManager() { return _clock_skew_management_manager; }
   StatisticsManager *getStatisticsManager()                   { return _statistics_manager; } 
   MCP *getMCP()                                               { return _mcp; }
   LCP *getLCP()                                               { return _lcp; }
   Config *getConfig()                                         { return &_config; }
   config::Config *getCfg()                                    { return _config_file; }

   void startTimer();
   void stopTimer();
   bool finished();

   std::string getGraphiteHome() const                         { return _graphite_home; }

   void enableModels();
   void disableModels();

   inline bool isEnabled() const                               { return _enabled; }

   static void enablePerformanceModelsInCurrentProcess();
   static void disablePerformanceModelsInCurrentProcess();

private:
   // Print final output of simulation
   void printSimulationSummary();

   // handle synchronization of shutdown for distributed simulator objects
   void broadcastFinish();
   void handleFinish(); // slave processes
   void deallocateProcess(); // master process

   void initializeGraphiteHome();

   void initializePowerModelingTools();
   void deInitializePowerModelingTools();

   void enableFrontEnd();
   void disableFrontEnd();

   Network& getMCPNetwork();
   
   friend class LCP;

   Config _config;
   Log _log;
   Transport *_transport;
   TileManager *_tile_manager;
   ThreadManager *_thread_manager;
   ThreadScheduler *_thread_scheduler;
   PerformanceCounterManager *_performance_counter_manager;
   SimThreadManager *_sim_thread_manager;
   ClockSkewManagementManager *_clock_skew_management_manager;
   StatisticsManager *_statistics_manager;

   MCP *_mcp;
   LCP *_lcp;

   bool _finished;
   UInt32 _num_procs_finished;

   UInt64 _boot_time;
   UInt64 _start_time;
   UInt64 _stop_time;
   UInt64 _shutdown_time;
   
   static config::Config *_config_file;

   std::string _graphite_home;

   bool _enabled;

   static Simulator *_singleton;
};

static inline Simulator* Sim()
{
   return Simulator::getSingleton();
}

#endif // SIMULATOR_H
