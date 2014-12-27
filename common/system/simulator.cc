#include <cstdlib>
#include <sstream>
#include <iomanip>

#include "simulator.h"
#include "version.h"
#include "log.h"
#include "lcp.h"
#include "mcp.h"
#include "tile.h"
#include "instruction.h"
#include "tile_manager.h"
#include "thread_manager.h"
#include "thread_scheduler.h"
#include "performance_counter_manager.h"
#include "sim_thread_manager.h"
#include "dvfs_manager.h"
#include "clock_skew_management_object.h"
#include "statistics_manager.h"
#include "statistics_thread.h"
#include "contrib/dsent/dsent_contrib.h"
#include "contrib/mcpat/cacti/io.h"

Simulator *Simulator::_singleton;
config::Config *Simulator::_config_file;

static UInt64 getTime()
{
   timeval t;
   gettimeofday(&t, NULL);
   UInt64 time = (((UInt64)t.tv_sec) * 1000000 + t.tv_usec);
   return time;
}

void Simulator::allocate()
{
   assert(_singleton == NULL);
   _singleton = new Simulator();
   assert(_singleton);
}

void Simulator::setConfig(config::Config *cfg)
{
   _config_file = cfg;
}

void Simulator::release()
{
   delete _singleton;
   _singleton = NULL;
}

Simulator* Simulator::getSingleton()
{
   return _singleton;
}

Simulator::Simulator()
   : _config()
   , _log(_config)
   , _transport(NULL)
   , _tile_manager(NULL)
   , _thread_manager(NULL)
   , _thread_scheduler(NULL)
   , _performance_counter_manager(NULL)
   , _sim_thread_manager(NULL)
   , _clock_skew_management_manager(NULL)
   , _statistics_manager(NULL)
   , _mcp(NULL)
   , _lcp(NULL)
   , _finished(false)
   , _boot_time(getTime())
   , _start_time(0)
   , _stop_time(0)
   , _shutdown_time(0)
   , _enabled(false)
{
}

void Simulator::start()
{
   LOG_PRINT("Simulator ctor starting...");
   _config.generateTileMap();

   initializeGraphiteHome();
   initializePowerModelingTools();
   DVFSManager::initialize();

   _transport = Transport::create();
   
   _tile_manager = new TileManager();
   _thread_manager = new ThreadManager(_tile_manager);
   _thread_scheduler = ThreadScheduler::create(_thread_manager, _tile_manager);
   _performance_counter_manager = new PerformanceCounterManager();
   _sim_thread_manager = new SimThreadManager();
   _clock_skew_management_manager = ClockSkewManagementManager::create(getCfg()->getString("clock_skew_management/scheme"));
   if (_config_file->getBool("statistics_trace/enabled"))
      _statistics_manager = new StatisticsManager();

   if (_config.isMasterProcess())   //sqc_multi
      _mcp = new MCP(getMCPNetwork());
   _lcp = new LCP();

   // Start threads needed for simulation
   _sim_thread_manager->spawnThreads();
   _lcp->spawnThread();
   if (_mcp)
      _mcp->spawnThread();
   if (_statistics_manager)
      _statistics_manager->spawnThread();

   shutdownPowerModelingTools();
   LOG_PRINT("Simulator ctor done...");
}

Simulator::~Simulator()
{
   LOG_PRINT("Simulator dtor starting...");
   _shutdown_time = getTime();

   broadcastFinish();

   // Quit threads needed for simulation
   if (_statistics_manager)
      _statistics_manager->quitThread();
   if (_mcp)
      _mcp->quitThread();
   _lcp->quitThread();

   _transport->barrier();
   printSimulationSummary();
   _sim_thread_manager->quitThreads();
   _transport->barrier();

   if (_statistics_manager)
      delete _statistics_manager;
   delete _lcp;
   if (_mcp)
      delete _mcp;
  
   if (_clock_skew_management_manager)
      delete _clock_skew_management_manager;
   delete _sim_thread_manager;
   delete _performance_counter_manager;
   delete _thread_manager;
   delete _thread_scheduler;
   delete _tile_manager;
   _tile_manager = NULL;
   delete _transport;
}

void Simulator::printSimulationSummary()
{
   // Byte::printSummary();
   if (Config::getSingleton()->getCurrentProcessNum() == 0)
   {
      ofstream os(Config::getSingleton()->getOutputFileName().c_str());

      os << "Graphite " << version  << endl << endl;
      os << "Simulation (Host) Timers: " << endl << left
         << setw(35) << "Start Time (in microseconds)" << (_start_time - _boot_time) << endl
         << setw(35) << "Stop Time (in microseconds)" << (_stop_time - _boot_time) << endl
         << setw(35) << "Shutdown Time (in microseconds)" << (_shutdown_time - _boot_time) << endl;

      _tile_manager->outputSummary(os);
      os.close();
   }
   else
   {
      stringstream temp;
      _tile_manager->outputSummary(temp);
      assert(temp.str().length() == 0);
   }
}

void Simulator::startTimer()
{
   _start_time = getTime();
}

void Simulator::stopTimer()
{
   _stop_time = getTime();
}

void Simulator::broadcastFinish()
{
   if (Config::getSingleton()->getCurrentProcessNum() != 0)
      return;

   _num_procs_finished = 1;

   // let the rest of the simulator know its time to exit
   Transport::Node *globalNode = Transport::getSingleton()->getGlobalNode();

   SInt32 msg = LCP_MESSAGE_SIMULATOR_FINISHED;
   for (UInt32 i = 1; i < Config::getSingleton()->getProcessCount(); i++)
   {
      globalNode->globalSend(i, &msg, sizeof(msg));
   }

   while (_num_procs_finished < Config::getSingleton()->getProcessCount())
   {
      sched_yield();
   }
}

void Simulator::handleFinish()
{
   LOG_ASSERT_ERROR(Config::getSingleton()->getCurrentProcessNum() != 0,
                    "LCP_MESSAGE_SIMULATOR_FINISHED received on master process.");

   Transport::Node *globalNode = Transport::getSingleton()->getGlobalNode();
   SInt32 msg = LCP_MESSAGE_SIMULATOR_FINISHED_ACK;
   globalNode->globalSend(0, &msg, sizeof(msg));

   _finished = true;
}

void Simulator::deallocateProcess()
{
   LOG_ASSERT_ERROR(Config::getSingleton()->getCurrentProcessNum() == 0,
                    "LCP_MESSAGE_SIMULATOR_FINISHED_ACK received on slave process.");

   ++_num_procs_finished;
}

bool Simulator::finished()
{
   return _finished;
}

void Simulator::initializeGraphiteHome()
{
   char* graphite_home_str = getenv("GRAPHITE_HOME");
   LOG_ASSERT_ERROR(graphite_home_str, "GRAPHITE_HOME environment variable NOT set");
   _graphite_home = (string) graphite_home_str; 
}

void Simulator::initializePowerModelingTools()
{
   if (!Config::getSingleton()->getEnablePowerModeling())
      return;
 
   // Initialize DSENT (network power modeling) - create config object
   string dsent_path = _graphite_home + "/contrib/dsent";
   dsent_contrib::DSENTInterface::allocate(dsent_path, getCfg()->getInt("general/technology_node"));
   dsent_contrib::DSENTInterface::getSingleton()->add_global_tech_overwrite("Temperature",
      getCfg()->getFloat("general/temperature"));

   // Initialize power models database
   initializePowerModelsDatabase();
}

void Simulator::initializePowerModelsDatabase()
{
   // Initialize database environment
   DBUtils::initializeEnv();
   // Initialize DSENT database (network power modeling)
   string dsent_path = _graphite_home + "/contrib/dsent";
   dsent_contrib::DSENTInterface::getSingleton()->initializeDatabase(dsent_path);
   // Initialize McPAT database (core + cache power modeling)
   string mcpat_path = _graphite_home + "/contrib/mcpat";
   McPAT::initializeDatabase(mcpat_path);
}

void Simulator::shutdownPowerModelingTools()
{
   if (!Config::getSingleton()->getEnablePowerModeling())
      return;

   // Shutdown power models database
   shutdownPowerModelsDatabase();
   
   // Release DSENT interface object
   dsent_contrib::DSENTInterface::release();
}

void Simulator::shutdownPowerModelsDatabase()
{
   // Shutdown DSENT database
   dsent_contrib::DSENTInterface::getSingleton()->shutdownDatabase();
   // Shutdown McPAT database
   McPAT::shutdownDatabase();
   // Shutdown database environment
   DBUtils::shutdownEnv();
}

Network& Simulator::getMCPNetwork()
{
   Tile* mcp_core = _tile_manager->getTileFromID(_config.getTargetMCPTileNum());   //sqc_multi
   fprintf(stderr, "Target MCP Tile Num (%i) \n", _config.getTargetMCPTileNum());   //sqc_multi
   LOG_ASSERT_ERROR(mcp_core, "Could not find the MCP's core");
   return *(mcp_core->getNetwork());
}

void Simulator::enableModels()
{
   LOG_PRINT("enableModels()");
   startTimer();
   _enabled = true;
   for (UInt32 i = 0; i < _config.getNumLocalTiles(); i++)
      _tile_manager->getTileFromIndex(i)->enableModels();
   enableFrontEnd();
}

void Simulator::disableModels()
{
   LOG_PRINT("disableModels()");
   stopTimer();
   _enabled = false;
   for (UInt32 i = 0; i < _config.getNumLocalTiles(); i++)
      _tile_manager->getTileFromIndex(i)->disableModels();
   disableFrontEnd();
}

__attribute__((weak))
void Simulator::enableFrontEnd()
{}

__attribute__((weak))
void Simulator::disableFrontEnd()
{}
