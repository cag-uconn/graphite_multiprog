#include "tile.h"
#include "core.h"
#include "core_model.h"
#include "models/in_order.h"
#include "models/out_of_order.h"
#include "branch_predictor.h"
#include "simulator.h"
#include "tile_manager.h"
#include "config.h"
#include "utils.h"
#include "time_types.h"
#include "mcpat_core_interface.h"
#include "remote_query_helper.h"
#include "memory_manager.h"

CoreModel* CoreModel::create(Core* core)
{
   string core_model = Config::getSingleton()->getCoreType(core->getTile()->getId());

   if (core_model == "in_order")
      return new InOrderCoreModel(core);
   else if (core_model == "out_of_order")
      return new OutOfOrderCoreModel(core);
   else
   {
      LOG_PRINT_ERROR("Invalid core model type: %s", core_model.c_str());
      return NULL;
   }
}

// Public Interface
CoreModel::CoreModel(Core *core)
   : _core(core)
   , _curr_time(0)
   , _instruction_count(0)
   , _average_frequency(0.0)
   , _total_time(0)
   , _checkpointed_time(0)
   , _total_cycles(0)
   , _instruction_queue(2)  // Max 2 instructions
   , _dynamic_memory_info_queue(3) // Max 3 dynamic memory request objects
   , _dynamic_branch_info_queue(1) // Max 1 dynamic branch info object
   , _enabled(false)
{
   // Create Branch Predictor
   _branch_predictor = BranchPredictor::create(this);

   // Initialize memory fence / dynamic instruction / pipeline stall Counters
   initializeMemoryFenceCounters();
   initializeStallCounters();

   // Initialize instruction costs
   initializeLatencyTable(_core->getFrequency());

   LOG_PRINT("Initialized CoreModel.");
}

CoreModel::~CoreModel()
{
   delete _mcpat_core_interface;
   delete _branch_predictor;
}

Time
CoreModel::getLatency(uint16_t lat) const
{
   return (lat < 16) ? _latency_table[lat] : Time( Latency((uint64_t) lat, _core->getFrequency()) );
}

void
CoreModel::initializeLatencyTable(double frequency)
{
   for (uint64_t i = 0; i < 16; i++)
      _latency_table[i] = Time( Latency(i, frequency) );
   _ONE_CYCLE = Time( Latency(1, _core->getFrequency()) );
}

void
CoreModel::updateLatencyTable(double frequency)
{
   for (uint64_t i = 0; i < 16; i++)
      _latency_table[i] = Time( Latency(i, frequency) );
   _ONE_CYCLE = Time( Latency(1, _core->getFrequency()) );
}

void
CoreModel::outputSummary(ostream& os, const Time& target_completion_time)
{
   os << "Core Summary:" << endl;
   os << "    Total Instructions: " << _instruction_count << endl;
   os << "    Completion Time (in nanoseconds): " << _curr_time.toNanosec() << endl;
   os << "    Average Frequency (in GHz): " << _average_frequency << endl;
   // Pipeline stall counters
   os << "    Stall Time Breakdown (in nanoseconds): " << endl;
   os << "      Instruction Fetch: "   << _total_instruction_fetch__stall_time.toNanosec()            << endl;
   os << "      Memory Access: "       << _total_memory_access__stall_time.toNanosec()                << endl;
   os << "        Load Queue: "        << _total_load_queue__stall_time.toNanosec()                   << endl;
   os << "        Store Queue: "       << _total_store_queue__stall_time.toNanosec()                  << endl;
   os << "      Execution Unit: "      << _total_execution_unit__stall_time.toNanosec()               << endl;
   os << "      Branch Speculation: "  << _total_branch_speculation_violation__stall_time.toNanosec() << endl;
   // os << "      Load Speculation: "    << _total_load_speculation_violation__stall_time.toNanosec()   << endl;
   os << "      Synchronization: "     << _total_sync__stall_time.toNanosec()                         << endl;
   os << "      Network Recv: "        << _total_netrecv__stall_time.toNanosec()                      << endl;
   os << "      Idle: "                << _total__idle_time.toNanosec()                               << endl;

   // Branch Predictor Summary
   if (_branch_predictor)
      _branch_predictor->outputSummary(os);

   _mcpat_core_interface->outputSummary(os, target_completion_time, _core->getFrequency());
   
   // Memory fence counters
   os << "    Fence Instructions: " << _total_fence_instructions << endl;
}

void
CoreModel::initializeMcPATInterface(UInt32 num_load_buffer_entries, UInt32 num_store_buffer_entries)
{
   // For Power/Area Modeling
   double frequency = _core->getFrequency();
   double voltage = _core->getVoltage();
   _mcpat_core_interface = new McPATCoreInterface(this, frequency, voltage, num_load_buffer_entries, num_store_buffer_entries);
}

void
CoreModel::updateMcPATCounters(Instruction* ins)
{
   // Get Branch Misprediction Count
   UInt64 total_branch_misprediction_count = _branch_predictor->getNumIncorrectPredictions();

   // Update Event Counters
   _mcpat_core_interface->updateEventCounters(ins->getMcPATInfo(),
                                              _curr_time.toCycles(_core->getFrequency()),
                                              total_branch_misprediction_count);
}

void
CoreModel::computeEnergy(const Time& curr_time)
{
   _mcpat_core_interface->computeEnergy(curr_time, _core->getFrequency());
}

double
CoreModel::getDynamicEnergy()
{
   return _mcpat_core_interface->getDynamicEnergy();
}

double
CoreModel::getLeakageEnergy()
{
   return _mcpat_core_interface->getLeakageEnergy();
}

void
CoreModel::enable()
{
   // Thread Spawner and MCP performance models should never be enabled
   if (_core->getTile()->getId() >= (tile_id_t) Config::getSingleton()->getApplicationTiles())
      return;
   _enabled = true;
}

void
CoreModel::disable()
{
   _enabled = false;
}

// This function is called:
// 1) Whenever frequency is changed
void
CoreModel::setDVFS(double old_frequency, double new_voltage, double new_frequency, const Time& curr_time)
{
   recomputeAverageFrequency(old_frequency);
   updateLatencyTable(new_frequency);
   _mcpat_core_interface->setDVFS(old_frequency, new_voltage, new_frequency, curr_time);
}

void
CoreModel::setCurrTime(Time time)
{
   _curr_time = time;
   _checkpointed_time = time;
}

// This function is called:
// 1) On thread exit
// 2) Whenever frequency is changed
void
CoreModel::recomputeAverageFrequency(double old_frequency)
{
   _total_cycles += (_curr_time - _checkpointed_time).toCycles(old_frequency);
   _total_time += (_curr_time - _checkpointed_time);
   _average_frequency = ((double) _total_cycles)/((double) _total_time.toNanosec());

   _checkpointed_time = _curr_time;
}

void
CoreModel::initializeMemoryFenceCounters()
{
   _total_fence_instructions = 0;
}

void
CoreModel::initializeStallCounters()
{
   _total_instruction_fetch__stall_time = Time(0);
   _total_memory_access__stall_time = Time(0);
   _total_load_queue__stall_time = Time(0);
   _total_store_queue__stall_time = Time(0);
   _total_execution_unit__stall_time = Time(0);
   _total_branch_speculation_violation__stall_time = Time(0);
   _total_load_speculation_violation__stall_time = Time(0);
   
   _total_netrecv__stall_time = Time(0);
   _total_sync__stall_time = Time(0);
   _total__idle_time = Time(0);
}

Time
CoreModel::issueInstructionFetch(const Time& issue_time, uintptr_t address, uint32_t size)
{
   Byte ins_buf[size];
   return _core->initiateMemoryAccess(MemComponent::L1_ICACHE, Core::NONE, Core::READ,
                                      address, ins_buf, size)._latency;
}

void
CoreModel::updateMemoryFenceCounters()
{
   _total_fence_instructions ++;
}

void
CoreModel::updateDynamicInstructionStallCounters(const DynamicInstruction* ins)
{
   switch (ins->getType())
   {
   case DynamicInstruction::NETRECV:
      _total_netrecv__stall_time += ins->getCost();
      break;
   case DynamicInstruction::SYNC:
      _total_sync__stall_time += ins->getCost();
      break;
   case DynamicInstruction::SPAWN:
      _total__idle_time += ins->getCost();
      break;
   default:
      LOG_PRINT_ERROR("Unrecognized dynamic instruction: %u", ins->getType());
      break;
   }
}

void
CoreModel::updatePipelineStallCounters(const Time& instruction_fetch__stall_time,
                                       const Time& memory_access__stall_time,
                                       const Time& load_queue__stall_time,
                                       const Time& store_queue__stall_time,
                                       const Time& execution_unit__stall_time,
                                       const Time& branch_speculation__violation_stall_time,
                                       const Time& load_speculation__violation_stall_time)
{
   _total_instruction_fetch__stall_time += instruction_fetch__stall_time;
   _total_memory_access__stall_time += memory_access__stall_time;
   _total_load_queue__stall_time += load_queue__stall_time;
   _total_store_queue__stall_time += store_queue__stall_time;
   _total_execution_unit__stall_time += execution_unit__stall_time;
   _total_branch_speculation_violation__stall_time += branch_speculation__violation_stall_time;
   _total_load_speculation_violation__stall_time += load_speculation__violation_stall_time;
}

void
CoreModel::processDynamicInstruction(DynamicInstruction* ins)
{
   if (_enabled)
   {
      LOG_PRINT("handleDynamicInstruction[Type(%s)]", ins->getTypeStr().c_str());
      handleDynamicInstruction(ins);
   }
   delete ins;
}

void
CoreModel::queueInstruction(Instruction* ins)
{
   if (!_enabled)
      return;
   assert(!_instruction_queue.full());
   _instruction_queue.push_back(ins);
}

void
CoreModel::iterate()
{
   while (_instruction_queue.size() > 1)
   {
      Instruction* ins = _instruction_queue.front();
      // Update number of instructions processed
      _instruction_count ++;
      
      LOG_PRINT("handleInstruction[Address(%#lx), Size(%u), Num-Uops(%u)]",
                ins->getAddress(), ins->getSize(), ins->getNumUops());
      handleInstruction(ins);
      _instruction_queue.pop_front();
   }
}

void
CoreModel::pushDynamicMemoryInfo(const DynamicMemoryInfo& info)
{
   if (_instruction_queue.empty() || !_enabled)
      return;
   LOG_PRINT("pushDynamicMemoryInfo[%s Address(%#lx), Size(%u)]",
             SPELL_MEMOP(info._mem_op_type), info._address, info._size);
   assert(!_dynamic_memory_info_queue.full());
   _dynamic_memory_info_queue.push_back(info);
}

void
CoreModel::popDynamicMemoryInfo()
{
   assert(_enabled);
   assert(!_dynamic_memory_info_queue.empty());
   __attribute__((unused)) const DynamicMemoryInfo& info = _dynamic_memory_info_queue.front();
   LOG_PRINT("popDynamicMemoryInfo[%s Address(%#lx), Size(%u)]",
             SPELL_MEMOP(info._mem_op_type), info._address, info._size);
   _dynamic_memory_info_queue.pop_front();
}

const
DynamicMemoryInfo& CoreModel::getDynamicMemoryInfo()
{
   return _dynamic_memory_info_queue.front();
}

void
CoreModel::pushDynamicBranchInfo(const DynamicBranchInfo& info)
{
   if (_instruction_queue.empty() || !_enabled)
      return;
   LOG_PRINT("pushDynamicBranchInfo[%s Target(%lx)]",
             info._taken ? "TAKEN" : "NOT-TAKEN", info._target);
   assert(!_dynamic_branch_info_queue.full());
   _dynamic_branch_info_queue.push_back(info);
}

void
CoreModel::popDynamicBranchInfo()
{
   assert(_enabled);
   assert(!_dynamic_branch_info_queue.empty());
   __attribute__((unused)) const DynamicBranchInfo& info = _dynamic_branch_info_queue.front();
   LOG_PRINT("popDynamicBranchInfo[%s Target(%lx)]",
             info._taken ? "TAKEN" : "NOT-TAKEN", info._target);
   _dynamic_branch_info_queue.pop_front();
}

const
DynamicBranchInfo& CoreModel::getDynamicBranchInfo()
{
   return _dynamic_branch_info_queue.front();
}
