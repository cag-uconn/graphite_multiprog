#include <cmath>
#include <cstring>
#include "core.h"
#include "tile.h"
#include "core_model.h"
#include "simulator.h"
#include "syscall_model.h"
#include "sync_client.h"
#include "network_types.h"
#include "memory_manager.h"
#include "pin_memory_manager.h"
#include "clock_skew_management_object.h"
#include "config.h"
#include "log.h"
#include "dvfs_manager.h"
#include "dynamic_memory_info.h"

Core::Core(Tile *tile, core_type_t core_type)
   : _tile(tile)
   , _core_model(NULL)
   , _state(IDLE)
   , _pin_memory_manager(NULL)
   , _enabled(false)
   , _module(CORE)
{

   //initialize frequency and voltage
   __attribute__((unused)) int rc = DVFSManager::getInitialFrequencyAndVoltage(CORE, _frequency, _voltage);
   LOG_ASSERT_ERROR(rc == 0, "Error setting initial voltage for frequency(%g)", _frequency);

   _id = (core_id_t) {_tile->getId(), core_type};
   if (Config::getSingleton()->getEnableCoreModeling())
      _core_model = CoreModel::create(this);

   _sync_client = new SyncClient(this);
   _syscall_model = new SyscallMdl(this);
   _clock_skew_management_client =
      ClockSkewManagementClient::create(Sim()->getCfg()->getString("clock_skew_management/scheme"), this);
 
   if (Config::getSingleton()->isSimulatingSharedMemory())
      _pin_memory_manager = new PinMemoryManager(this);

   initializeMemoryAccessLatencyCounters();
   initializeInstructionBuffer();

   // asynchronous communication
   _synchronization_delay = Time(Latency(DVFSManager::getSynchronizationDelay(), _frequency));
   _asynchronous_map[L1_ICACHE] = Time(0);
   _asynchronous_map[L1_DCACHE] = Time(0);

   LOG_PRINT("Initialized Core.");
}

Core::~Core()
{
   if (_pin_memory_manager)
      delete _pin_memory_manager;

   if (_clock_skew_management_client)
      delete _clock_skew_management_client;

   delete _syscall_model;
   delete _sync_client;
   delete _core_model;
}

int
Core::coreSendW(int sender, int receiver, char* buffer, int size, carbon_network_t net_type)
{
   PacketType packet_type = getPacketTypeFromUserNetType(net_type);

   core_id_t receiver_core = (core_id_t) {receiver, _id.core_type};

   SInt32 sent = (receiver == CAPI_ENDPOINT_ALL) ?
                 _tile->getNetwork()->netBroadcast(packet_type, buffer, size) :
                 _tile->getNetwork()->netSend(receiver_core, packet_type, buffer, size);
   
   LOG_ASSERT_ERROR(sent == size, "Bytes Sent(%i), Message Size(%i)", sent, size);

   return (sent == size) ? 0 : -1;
}

int
Core::coreRecvW(int sender, int receiver, char* buffer, int size, carbon_network_t net_type)
{
   PacketType packet_type = getPacketTypeFromUserNetType(net_type);

   core_id_t sender_core = (core_id_t) {sender, _id.core_type};

   NetPacket packet = (sender == CAPI_ENDPOINT_ANY) ?
                      _tile->getNetwork()->netRecvType(packet_type, _id) :
                      _tile->getNetwork()->netRecv(sender_core, _id, packet_type);

   LOG_PRINT("Got packet: from (%i,%i), to (%i,%i), type (%i), len (%i)",
             packet.sender.tile_id, packet.sender.core_type, packet.receiver.tile_id, packet.receiver.core_type,
             (SInt32) packet.type, packet.length);

   LOG_ASSERT_ERROR((unsigned)size == packet.length,
                    "Application requested packet of size: %d, got a packet from %d of size: %d",
                    size, sender, packet.length);

   memcpy(buffer, packet.data, size);

   // De-allocate dynamic memory
   // Is this the best place to de-allocate packet.data ??
   delete [] (Byte*) packet.data;

   return (unsigned)size == packet.length ? 0 : -1;
}

// accessMemory(lock_signal_t lock_signal, mem_op_t mem_op_type, IntPtr address, char* data_buffer, UInt32 data_size, bool push_info)
//
// Arguments:
//   lock_signal :: NONE, LOCK, or UNLOCK
//   mem_op_type :: READ, READ_EX, or WRITE
//   address :: address of location we want to access (read or write)
//   data_buffer :: buffer holding data for WRITE or buffer which must be written on a READ
//   data_size :: size of data we must read/write
//   push_info :: says whether memory info must be pushed to the core model
//
// Return Value:
//   (number of misses, memory access latency) :: the number of cache misses and memory access latency


void
Core::accessMemory(lock_signal_t lock_signal, mem_op_t mem_op_type, IntPtr address, char* data_buffer, UInt32 data_size, bool push_info)
{
   initiateMemoryAccess(MemComponent::L1_DCACHE, lock_signal, mem_op_type, address, (Byte*) data_buffer, data_size, push_info);
}

Time
Core::readInstructionMemory(IntPtr address, UInt32 instruction_size)
{
   Byte buf[instruction_size];
   return initiateMemoryAccess(MemComponent::L1_ICACHE, Core::NONE, Core::READ, address, buf, instruction_size)._latency;
}

DynamicMemoryInfo
Core::initiateMemoryAccess(MemComponent::Type mem_component, lock_signal_t lock_signal, mem_op_t mem_op_type,
                           IntPtr address, Byte* data_buf, UInt32 data_size,
                           bool push_info, Time time_arg)
{
   // Accommodate the target ID also within the address
   address = address | (IntPtr) Config::getSingleton()->getCurrentTargetNum() << 48;

   LOG_ASSERT_ERROR(Config::getSingleton()->isSimulatingSharedMemory(), "Shared Memory Disabled");
   DynamicMemoryInfo dynamic_memory_info(address, data_size, mem_op_type, lock_signal);

   if (data_size == 0)
   {
      LOG_PRINT("Mem-Component(%s), Lock-Signal(%s), Mem-Op-Type(%s), Address(%#lx), Data-Size(%u)",
                SPELL_MEMCOMP(mem_component), SPELL_LOCK_SIGNAL(lock_signal), SPELL_MEMOP(mem_op_type), address, data_size);
      
      if (_core_model && push_info)
         _core_model->pushDynamicMemoryInfo(dynamic_memory_info);
      return dynamic_memory_info;
   }

   // Setting the initial time
   Time curr_time = time_arg;
   if (curr_time == 0)
      curr_time = (lock_signal == Core::UNLOCK) ? _lock_acquire_time : _core_model->getCurrTime();

   LOG_PRINT("Time(%llu), %s - ADDR(%#lx), data_size(%u), START",
             curr_time.toNanosec(), ((mem_op_type == READ) ? "READ" : "WRITE"), address, data_size);

   UInt32 cache_line_size = _tile->getMemoryManager()->getCacheLineSize();

   IntPtr begin_addr = address;
   IntPtr end_addr = address + data_size;
   IntPtr begin_addr_aligned = begin_addr - (begin_addr % cache_line_size);
   IntPtr end_addr_aligned = end_addr - (end_addr % cache_line_size);
   Byte *curr_data_buffer_head = (Byte*) data_buf;

   for (IntPtr curr_addr_aligned = begin_addr_aligned; curr_addr_aligned <= end_addr_aligned; curr_addr_aligned += cache_line_size)
   {
      // Access the cache one line at a time
      UInt32 curr_offset;
      UInt32 curr_size;

      // Determine the offset
      curr_offset = (curr_addr_aligned == begin_addr_aligned) ? (begin_addr % cache_line_size) : 0;

      // Determine the size
      if (curr_addr_aligned == end_addr_aligned)
      {
         curr_size = (end_addr % cache_line_size) - (curr_offset);
         if (curr_size == 0)
            continue;
      }
      else
      {
         curr_size = cache_line_size - curr_offset;
      }

      // Check Instruction Buffer
      if (mem_component == MemComponent::L1_ICACHE)
      {
         LOG_ASSERT_ERROR(_enabled, "Models not enabled -> Access to L1_ICACHE not possible");
         if (_instruction_buffer_address == curr_addr_aligned)
         {
            // Instruction buffer hit, so NO need to access ICACHE
            _instruction_buffer_hits ++;
            // 1 cycle to access instruction buffer
            Time access_time = Latency(1, _frequency);
            curr_time += access_time;
            dynamic_memory_info._latency += access_time;
            continue;
         }
         else
         {
            // Instruction buffer miss, so need to access ICACHE
            _instruction_buffer_address = curr_addr_aligned;
         }
      }

      LOG_PRINT("Start coreInitiateMemoryAccess: ADDR(%#lx), offset(%u), curr_size(%u), core_id(%i, %i)",
                curr_addr_aligned, curr_offset, curr_size, getId().tile_id, getId().core_type);

      // If it is a READ or READ_EX operation, 
      //    'coreInitiateMemoryAccess' causes curr_data_buffer_head to be automatically filled in
      // If it is a WRITE operation, 
      //    'coreInitiateMemoryAccess' reads the data from curr_data_buffer_head
      _tile->getMemoryManager()->__coreInitiateMemoryAccess(mem_component, lock_signal, mem_op_type, 
                                                            curr_addr_aligned, curr_offset, 
                                                            curr_data_buffer_head, curr_size,
                                                            curr_time, dynamic_memory_info);
      LOG_PRINT("End InitiateSharedMemReq: ADDR(%#lx), offset(%u), curr_size(%u), core_id(%i,%i)",
                curr_addr_aligned, curr_offset, curr_size, getId().tile_id, getId().core_type);

      // Increment the buffer head
      curr_data_buffer_head += curr_size;

      // Add synchronization delay
      curr_time += getSynchronizationDelay(DVFSManager::convertToModule(mem_component));
   }

   // Get the final cycle time
   Time final_time = curr_time;
 
   // Set time of lock acquire 
   if (lock_signal == Core::LOCK)
      _lock_acquire_time = final_time;

   LOG_PRINT("Time(%llu), %s - ADDR(%#lx), data_size(%u), END", 
             final_time.toNanosec(), ((mem_op_type == READ) ? "READ" : "WRITE"), address, data_size);

   // Calculate the round-trip time
   incrTotalMemoryAccessLatency(mem_component, dynamic_memory_info._latency);
   
   if (_core_model && push_info)
      _core_model->pushDynamicMemoryInfo(dynamic_memory_info);

   return dynamic_memory_info;
}

PacketType
Core::getPacketTypeFromUserNetType(carbon_network_t net_type)
{
   switch (net_type)
   {
   case CARBON_NET_USER:
      return USER;

   default:
      LOG_PRINT_ERROR("Unrecognized User Network(%u)", net_type);
      return (PacketType) -1;
   }
}

void
Core::outputSummary(ostream& os, const Time& target_completion_time)
{
   if (_core_model)
      _core_model->outputSummary(os, target_completion_time);
   
   DVFSManager::printAsynchronousMap(os, _module, _asynchronous_map);
   
   UInt64 total_instruction_memory_access_latency_in_ns = _total_instruction_memory_access_latency.toNanosec();
   UInt64 total_data_memory_access_latency_in_ns = _total_data_memory_access_latency.toNanosec();
   
   os << "Shared Memory Model Summary: " << endl;
   os << "    Total Memory Accesses: " << _num_instruction_memory_accesses + _num_data_memory_accesses << endl;
   os << "    Average Memory Access Latency (in nanoseconds): "
      << (1.0 * (total_instruction_memory_access_latency_in_ns + total_data_memory_access_latency_in_ns) /
                (_num_instruction_memory_accesses + _num_data_memory_accesses))
      << endl;
   
   os << "    Total Instruction Memory Accesses: " << _num_instruction_memory_accesses << endl;
   os << "    Instruction Buffer Hits: " << _instruction_buffer_hits << endl;
   os << "    Average Instruction Memory Access Latency (in nanoseconds): "
      << 1.0 * total_instruction_memory_access_latency_in_ns / _num_instruction_memory_accesses
      << endl;
   
   os << "    Total Data Memory Accesses: " << _num_data_memory_accesses << endl;
   os << "    Average Data Memory Access Latency (in nanoseconds): "
      << 1.0 * total_data_memory_access_latency_in_ns / _num_data_memory_accesses
      << endl;
}

void
Core::enableModels()
{
   _enabled = true;
   if (_core_model)
      _core_model->enable();
   if (_clock_skew_management_client)
      _clock_skew_management_client->enable();
}

void
Core::disableModels()
{
   _enabled = false;
   if (_core_model)
      _core_model->disable();
   if (_clock_skew_management_client)
      _clock_skew_management_client->disable();
}

void
Core::initializeInstructionBuffer()
{
   _instruction_buffer_address = INVALID_ADDRESS;
   _instruction_buffer_hits = 0;
}

void
Core::initializeMemoryAccessLatencyCounters()
{
   _num_instruction_memory_accesses = 0;
   _total_instruction_memory_access_latency = Time(0);
   _num_data_memory_accesses = 0;
   _total_data_memory_access_latency = Time(0);
}

void
Core::incrTotalMemoryAccessLatency(MemComponent::Type mem_component, Time memory_access_latency)
{
   if (!_enabled)
      return;

   if (mem_component == MemComponent::L1_ICACHE)
   {
      _num_instruction_memory_accesses ++;
      _total_instruction_memory_access_latency += memory_access_latency;
   }
   else if (mem_component == MemComponent::L1_DCACHE)
   {
      _num_data_memory_accesses ++;
      _total_data_memory_access_latency += memory_access_latency;
   }
   else
   {
      LOG_PRINT_ERROR("Unrecognized mem component(%s)", SPELL_MEMCOMP(mem_component));
   }
}

string
Core::spellMemOp(mem_op_t mem_op_type)
{
   switch (mem_op_type)
   {
   case READ:
      return "READ";
   case READ_EX:
      return "READ_EX";
   case WRITE:
      return "WRITE";
   default:
      LOG_PRINT_ERROR("Unrecognized mem op type: %u", mem_op_type);
      return "";
   }
}

string
Core::spellLockSignal(lock_signal_t lock_signal)
{
   switch (lock_signal)
   {
   case NONE:
      return "NONE";
   case LOCK:
      return "LOCK";
   case UNLOCK:
      return "UNLOCK";
   default:
      LOG_PRINT_ERROR("Unrecognized lock signal: %u", lock_signal);
      return "";
   }
}

int
Core::getDVFS(double &frequency, double &voltage)
{
   frequency = _frequency;
   voltage = _voltage;
   return 0;
}

int
Core::setDVFS(double frequency, voltage_option_t voltage_flag, const Time& curr_time)
{
   int rc = DVFSManager::getVoltage(_voltage, voltage_flag, frequency);

   if (rc==0)
   {
      _core_model->setDVFS(_frequency, _voltage, frequency, curr_time);
      _frequency = frequency;
      _synchronization_delay = Time(Latency(DVFSManager::getSynchronizationDelay(), _frequency));
   }
   return rc;
}

Time
Core::getSynchronizationDelay(module_t module)
{
   if (!DVFSManager::hasSameDVFSDomain(_module, module) && _enabled){
      _asynchronous_map[module] += _synchronization_delay;
      return _synchronization_delay;
   }
   return Time(0);
}
