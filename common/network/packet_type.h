#ifndef __PACKET_TYPE_H__
#define __PACKET_TYPE_H__

enum PacketType
{
   INVALID_PACKET_TYPE,
   USER_1,
   USER_2,
   SHARED_MEM_1,
   SHARED_MEM_2,
   SIM_THREAD_TERMINATE_THREADS,
   MCP_REQUEST_TYPE,
   MCP_RESPONSE_TYPE,
   MCP_UTILIZATION_UPDATE_TYPE,
   MCP_SYSTEM_TYPE,
   MCP_SYSTEM_RESPONSE_TYPE,
   MCP_THREAD_SPAWN_REPLY_FROM_MASTER_TYPE,
   MCP_THREAD_JOIN_REPLY,
   LCP_COMM_ID_UPDATE_REPLY,
   SYSTEM_INITIALIZATION_NOTIFY,
   SYSTEM_INITIALIZATION_ACK,
   SYSTEM_INITIALIZATION_FINI,
   CLOCK_SKEW_MINIMIZATION,
   RESET_CACHE_COUNTERS,   // Deprecated
   DISABLE_CACHE_COUNTERS, // Deprecated
   NUM_PACKET_TYPES
};

// This defines the different static network types
enum EStaticNetwork
{
   STATIC_NETWORK_USER_1,
   STATIC_NETWORK_USER_2,
   STATIC_NETWORK_MEMORY_1,
   STATIC_NETWORK_MEMORY_2,
   STATIC_NETWORK_SYSTEM,
   NUM_STATIC_NETWORKS
};

// Packets are routed to a static network based on their type. This
// gives the static network to use for a given packet type.
static EStaticNetwork g_type_to_static_network_map[] __attribute__((unused)) =
{
   STATIC_NETWORK_SYSTEM,        // INVALID_PACKET_TYPE
   STATIC_NETWORK_USER_1,        // USER_1
   STATIC_NETWORK_USER_2,        // USER_2
   STATIC_NETWORK_MEMORY_1,      // SM_1
   STATIC_NETWORK_MEMORY_2,      // SM_2
   STATIC_NETWORK_SYSTEM,        // ST_TERMINATE_THREADS
   STATIC_NETWORK_USER_1,        // MCP_REQ
   STATIC_NETWORK_USER_1,        // MCP_RESP
   STATIC_NETWORK_SYSTEM,        // MCP_UTIL
   STATIC_NETWORK_SYSTEM,        // MCP_SYSTEM
   STATIC_NETWORK_SYSTEM,        // MCP_SYSTEM_RESP
   STATIC_NETWORK_SYSTEM,        // MCP_THREAD_SPAWN
   STATIC_NETWORK_SYSTEM,        // MCP_THREAD_JOIN
   STATIC_NETWORK_SYSTEM,        // LCP_COMM_ID
   STATIC_NETWORK_SYSTEM,        // SYSTEM_INITIALIZATION_NOTIFY
   STATIC_NETWORK_SYSTEM,        // SYSTEM_INITIALIZATION_ACK
   STATIC_NETWORK_SYSTEM,        // SYSTEM_INITIALIZATION_FINI
   STATIC_NETWORK_SYSTEM,        // CLOCK_SKEW_MINIMIZATION
   STATIC_NETWORK_SYSTEM,        // RESET_CACHE_COUNTERS
   STATIC_NETWORK_SYSTEM         // DISABLE_CACHE_COUNTERS
};

#endif
