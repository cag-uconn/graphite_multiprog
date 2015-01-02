#include "performance_counter_support.h"
#include "performance_counter_manager.h"
#include "simulator.h"
#include "tile_manager.h"
#include "tile.h"
#include "core.h"
#include "network.h"
#include "transport.h"
#include "packetize.h"
#include "message_types.h"
#include "log.h"

void CarbonEnableModels()
{
   if (Sim()->getCfg()->getBool("general/trigger_models_within_application", false))
      __CarbonEnableModels();
}

void CarbonDisableModels()
{
   if (Sim()->getCfg()->getBool("general/trigger_models_within_application", false))
      __CarbonDisableModels();
}

void __CarbonEnableModels()
{
   printf("[[Graphite]] --> [ Enabling Performance and Power Models ]\n");
   fflush(stdout);
   
   Core* core = Sim()->getTileManager()->getCurrentCore();
   Network* network = core->getTile()->getNetwork();
   // Send a message to the Master MCP asking it to initialize the models
   UnstructuredBuffer send_buff;
   send_buff << (SInt32) MCP_MESSAGE_TOGGLE_PERFORMANCE_COUNTERS << (SInt32) PerformanceCounterManager::ENABLE;
   network->netSend(Config::getSingleton()->getMasterMCPCoreID(),
                    MCP_SYSTEM_TYPE,
                    send_buff.getBuffer(),
                    send_buff.size());

   // Wait for the Master MCP to reply after initializing all simulator models
   NetPacket pkt = network->netRecvType(MCP_SYSTEM_RESPONSE_TYPE, core->getId());
}

void __CarbonDisableModels()
{
   printf("[[Graphite]] --> [ Disabling Performance and Power Models ]\n");
   fflush(stdout);
   
   Core* core = Sim()->getTileManager()->getCurrentCore();
   Network* network = core->getTile()->getNetwork();
   // Send a message to the Master MCP asking it to initialize the models
   UnstructuredBuffer send_buff;
   send_buff << (SInt32) MCP_MESSAGE_TOGGLE_PERFORMANCE_COUNTERS << (SInt32) PerformanceCounterManager::DISABLE;
   network->netSend(Config::getSingleton()->getMasterMCPCoreID(),
                    MCP_SYSTEM_TYPE,
                    send_buff.getBuffer(),
                    send_buff.size());

   // Wait for the Master MCP to reply after initializing all simulator models
   NetPacket pkt = network->netRecvType(MCP_SYSTEM_RESPONSE_TYPE, core->getId());
} 
