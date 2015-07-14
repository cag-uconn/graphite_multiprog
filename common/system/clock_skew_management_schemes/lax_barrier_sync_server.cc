#include "lax_barrier_sync_client.h"
#include "lax_barrier_sync_server.h"
#include "simulator.h"
#include "thread_manager.h"
#include "tile_manager.h"
#include "network.h"
#include "tile.h"
#include "config.h"
#include "statistics_manager.h"
#include "statistics_thread.h"
#include "log.h"

LaxBarrierSyncServer::LaxBarrierSyncServer(Network &network, UnstructuredBuffer &recv_buff):
   m_network(network),
   m_recv_buff(recv_buff)
{
   m_thread_manager = Sim()->getThreadManager();
   try
   {
      m_barrier_interval = (UInt64) Sim()->getCfg()->getInt("clock_skew_management/lax_barrier/quantum"); 
   }
   catch(...)
   {
      LOG_PRINT_ERROR("Error Reading 'clock_skew_management/barrier/quantum' from the config file");
   }

   m_next_barrier_time = m_barrier_interval;
   m_num_application_tiles = Config::getSingleton()->getApplicationTiles();
   m_num_targets = Config::getSingleton()->getTargetCount();
   m_num_application_current_target_tiles = Config::getSingleton()->getApplicationTilesCurrentTarget();
   m_local_clock_list.resize(m_num_application_current_target_tiles);
   m_barrier_acquire_list.resize(m_num_application_current_target_tiles);
   m_local_mcp_barrier_acquire = 0;
   m_target_running_status_list.resize(m_num_targets);

//   for (UInt32 i = 0; i < m_num_application_tiles; i++)
   for (UInt32 i = 0; i < m_num_application_current_target_tiles; i++)  //sqc
   {
      m_local_clock_list[i] = 0;
      m_barrier_acquire_list[i] = false;
   }
   
   for (UInt32 i = 0; i < m_num_targets; i++)
   {
      m_target_running_status_list[i] = true;
   }
}

LaxBarrierSyncServer::~LaxBarrierSyncServer()
{}

void
LaxBarrierSyncServer::processSyncMsgLocal(core_id_t core_id)
{
   barrierWait(core_id);
}

void
LaxBarrierSyncServer::processSyncMsgGlobal(core_id_t core_id)
{
   m_local_mcp_barrier_acquire ++;
   UInt32 num_running_targets = 0;
   
   for(UInt32 i=0 ; i <m_num_targets; i++) 
   {
      if (m_target_running_status_list[i] == true)
         num_running_targets ++;
   }

   if (m_local_mcp_barrier_acquire == num_running_targets)
   {
      tile_id_t tile_id = Config::getSingleton()->getMasterMCPTileID();
      for(UInt32 i=0 ; i <m_num_targets; i++) 
      {
         UnstructuredBuffer m_send_buff;
         int msg_type = MCP_MESSAGE_CLOCK_SKEW_MANAGEMENT_GLOBAL_ACK;
         m_send_buff << msg_type;
         if (m_target_running_status_list[i] == true)
            m_network.netSend((core_id_t) {(tile_id_t) (tile_id+i), MAIN_CORE_TYPE}, MCP_SYSTEM_TYPE, m_send_buff.getBuffer(), m_send_buff.size());
      }  
      m_local_mcp_barrier_acquire = 0;
   }
}

void
LaxBarrierSyncServer::processSyncMsgGlobalAck(core_id_t core_id)
{
   barrierRelease();
}

void
LaxBarrierSyncServer::signal()
{
   if (isBarrierReachedLocal())
   {
      UnstructuredBuffer m_send_buff;
      int msg_type = MCP_MESSAGE_CLOCK_SKEW_MANAGEMENT_GLOBAL;
      m_send_buff << msg_type;
      m_network.netSend(Config::getSingleton()->getMasterMCPCoreID(), MCP_SYSTEM_TYPE, m_send_buff.getBuffer(), m_send_buff.size());

//     barrierRelease(); 
   }
}

void
LaxBarrierSyncServer::barrierWait(core_id_t core_id)
{
   UInt64 time_ns;
   m_recv_buff >> time_ns;

   SInt32 tile_idx = m_thread_manager->getTileIDXFromTileID(core_id.tile_id);

   LOG_PRINT("Received 'SIM_BARRIER_WAIT' from Core(%i, %i), Time(%llu)", core_id.tile_id, core_id.core_type, time_ns);

   LOG_ASSERT_ERROR(m_thread_manager->getRunningThreadIDX(core_id) != INVALID_THREAD_ID || m_thread_manager->isCoreInitializing(core_id) != INVALID_THREAD_ID,
                    "Thread on core(%i) is not running or initializing at time(%llu)", core_id, time_ns);

   if (time_ns < m_next_barrier_time)
   {
      LOG_PRINT("Sent 'SIM_BARRIER_RELEASE' immediately time(%llu), m_next_barrier_time(%llu)", time, m_next_barrier_time);
      // LOG_PRINT_WARNING("tile_id(%i), local_clock(%llu), m_next_barrier_time(%llu), m_barrier_interval(%llu)", tile_id, time, m_next_barrier_time, m_barrier_interval);
      unsigned int reply = LaxBarrierSyncClient::BARRIER_RELEASE;

      m_network.netSend(core_id, MCP_SYSTEM_RESPONSE_TYPE, (char*) &reply, sizeof(reply));
      return;
   }

   if (core_id.core_type == MAIN_CORE_TYPE)
   {
     // m_local_clock_list[core_id.tile_id] = time_ns;
     // m_barrier_acquire_list[core_id.tile_id] = true;
      m_local_clock_list[tile_idx] = time_ns;
      m_barrier_acquire_list[tile_idx] = true;
   }
   else
      LOG_ASSERT_ERROR(false, "Invalid core type!");
 
   signal(); 
}

bool
LaxBarrierSyncServer::isBarrierReachedLocal()
{
   bool single_thread_barrier_reached = false;

   // Check if all threads have reached the barrier
   // All least one thread must have (sync_time > m_next_barrier_time)
  // for (tile_id_t tile_id = 0; tile_id < (tile_id_t) m_num_application_tiles; tile_id++)
   for (SInt32 tile_idx = 0; tile_idx < (SInt32) m_num_application_current_target_tiles; tile_idx++)
   {
      tile_id_t tile_id = m_thread_manager->getTileIDFromTileIDX(tile_idx);
      if (m_local_clock_list[tile_idx] < m_next_barrier_time)
      {
         if (m_thread_manager->getRunningThreadIDX(tile_id) != INVALID_THREAD_ID)
         {
            // Thread Running on this core has not reached the barrier
            // Wait for it to sync
            return false;
         }
      }
      else
      {
         LOG_ASSERT_ERROR(m_thread_manager->getRunningThreadIDX(tile_id) != INVALID_THREAD_ID  || m_thread_manager->isCoreInitializing(tile_id) != INVALID_THREAD_ID , "Thread on core(%i) is not running or initializing at local_clock(%llu), m_next_barrier_time(%llu)", tile_id, m_local_clock_list[tile_id], m_next_barrier_time);

         // At least one thread has reached the barrier
         single_thread_barrier_reached = true;
      }
   }

   return single_thread_barrier_reached;
}

void
LaxBarrierSyncServer::barrierRelease()
{
   LOG_PRINT("Sending 'BARRIER_RELEASE'");

   // All threads have reached the barrier
   // Advance m_next_barrier_time
   // Release the Barrier
   
   // If a thread cannot be resumed, we have to advance the sync 
   // time till a thread can be resumed. Then only, will we have 
   // forward progress

   bool thread_resumed = false;
   while (!thread_resumed)
   {
      m_next_barrier_time += m_barrier_interval;
      LOG_PRINT("m_next_barrier_time updated to (%llu)", m_next_barrier_time);

     // for (tile_id_t tile_id = 0; tile_id < (tile_id_t) m_num_application_tiles; tile_id++)
      for (SInt32 tile_idx = 0; tile_idx < (SInt32) m_num_application_current_target_tiles; tile_idx++)
      {
         tile_id_t tile_id = m_thread_manager->getTileIDFromTileIDX(tile_idx);
         if (m_local_clock_list[tile_idx] < m_next_barrier_time)
         {
            // Check if this core was running. If yes, send a message to that core
            if (m_barrier_acquire_list[tile_idx] == true)
            {
               LOG_ASSERT_ERROR(m_thread_manager->getRunningThreadIDX(tile_id) != INVALID_THREAD_ID || m_thread_manager->isCoreInitializing(tile_id) != INVALID_THREAD_ID, "(%i) has acquired barrier, local_clock(%i), m_next_barrier_time(%llu), but not initializing or running", tile_id, m_local_clock_list[tile_idx], m_next_barrier_time);

               unsigned int reply = LaxBarrierSyncClient::BARRIER_RELEASE;

               m_network.netSend(Tile::getMainCoreId(tile_id), MCP_SYSTEM_RESPONSE_TYPE, (char*) &reply, sizeof(reply));

               m_barrier_acquire_list[tile_idx] = false;

               thread_resumed = true;
            }
         }
      }
   }

   LOG_PRINT("Barrier-Release: Next-Time(%llu)", m_next_barrier_time);

   // Notify Statistics thread about the global time
   if (Sim()->getStatisticsManager())
      Sim()->getStatisticsManager()->getThread()->notify(m_next_barrier_time);
}
void
LaxBarrierSyncServer::setTargetRunningStatus(UInt32 target_id, bool status)
{
   m_target_running_status_list[target_id] = status;

   UInt32 num_running_targets = 0; 

   for(UInt32 i=0 ; i <m_num_targets; i++)
   {
      if (m_target_running_status_list[i] == true)
      {
         num_running_targets ++;
         LOG_PRINT("Checking target running status, targe %i is running",  i);
      }
   }

   if (m_local_mcp_barrier_acquire == num_running_targets)
   {
      tile_id_t tile_id = Config::getSingleton()->getMasterMCPTileID();
      LOG_PRINT("all target get to barrier");
      for(UInt32 i=0 ; i <m_num_targets; i++)
      {
         UnstructuredBuffer m_send_buff;
         int msg_type = MCP_MESSAGE_CLOCK_SKEW_MANAGEMENT_GLOBAL_ACK;
         m_send_buff << msg_type;
         if (m_target_running_status_list[i] == true)
            m_network.netSend((core_id_t) {(tile_id_t) (tile_id+i), MAIN_CORE_TYPE}, MCP_SYSTEM_TYPE, m_send_buff.getBuffer(), m_send_buff.size());
      }
      m_local_mcp_barrier_acquire = 0;
   }

}
