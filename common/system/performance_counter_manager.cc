#include "performance_counter_manager.h"
#include "simulator.h"
#include "tile_manager.h"
#include "network.h"
#include "tile.h"
#include "core.h"
#include "transport.h"
#include "packetize.h"
#include "message_types.h"
#include "log.h"

PerformanceCounterManager::PerformanceCounterManager()
   : _num_toggle_requests_received(0)
   , _num_toggle_responses_received(0)
{}

PerformanceCounterManager::~PerformanceCounterManager()
{}

void
PerformanceCounterManager::masterTogglePerformanceCountersRequest(Byte* msg)
{
   SInt32 msg_type = *((SInt32*) msg);
   Config* config = Config::getSingleton(); 
   
   LOG_PRINT("Processing message in masterTogglePerformanceCounterRequester()  -- type : %i", msg_type);
   // If received message from all targets, proceed to initialize models.
   // Else, wait till this is received from all targets
   _num_toggle_requests_received ++;
   if (_num_toggle_requests_received == config->getTargetCount())
   {
      Transport::Node *transport = Transport::getSingleton()->getGlobalNode();
      UnstructuredBuffer send_buff;
      send_buff << (SInt32) LCP_MESSAGE_TOGGLE_PERFORMANCE_COUNTERS << msg_type;

      // Send message to all processes to enable models
      for (SInt32 i = 0; i < (SInt32) Config::getSingleton()->getProcessCount(); i++)
         transport->globalSend(i, send_buff.getBuffer(), send_buff.size());

      _num_toggle_requests_received = 0;
   }
}

void
PerformanceCounterManager::masterTogglePerformanceCountersResponse()
{
   Config* config = Config::getSingleton(); 
   
   // If received responses from all processes, proceed to reply to requester
   // Else, wait till all responses are received
   _num_toggle_responses_received ++;
   if (_num_toggle_responses_received == config->getProcessCount())
   {
      Core* core = Sim()->getTileManager()->getCurrentCore();
      Network* network = core->getTile()->getNetwork();
      for (UInt32 i = 0; i < config->getTargetCount(); i++)
      {
         network->netSend(Tile::getMainCoreId(config->getMasterThreadTileIDForTarget(i)),
                          MCP_SYSTEM_RESPONSE_TYPE,
                          NULL, 0);
      }

      _num_toggle_responses_received = 0;
   }
}

void
PerformanceCounterManager::togglePerformanceCounters(Byte* msg)
{
   SInt32 msg_type = *((SInt32*) msg);
   switch (msg_type)
   {
   case ENABLE:
      Sim()->enableModels();
      break;
   case DISABLE:
      Sim()->disableModels();
      break;
   default:
      LOG_PRINT_ERROR("Unrecognized msg type(%i)", msg_type);
      break;
   }

   // Send ACK back to master MCP
   SInt32 message_type = MCP_MESSAGE_TOGGLE_PERFORMANCE_COUNTERS_ACK;
   NetPacket ack(Time(0) /* time */, MCP_SYSTEM_TYPE /* packet type */,
                 0 /* sender - doesn't matter */,Config::getSingleton()->getMasterMCPTileID() /* receiver */,
                 sizeof(message_type) /* length */, &message_type /* data */);
   
   Byte buffer[ack.bufferSize()];
   ack.makeBuffer(buffer);

   Transport::Node* transport = Transport::getSingleton()->getGlobalNode();
   transport->send(Config::getSingleton()->getMasterMCPTileID(), buffer, ack.bufferSize());
}
