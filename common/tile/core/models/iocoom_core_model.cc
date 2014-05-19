#include "core.h"
#include "iocoom_core_model.h"
#include "config.hpp"
#include "simulator.h"
#include "branch_predictor.h"
#include "tile.h"
#include "utils.h"
#include "log.h"

IOCOOMCoreModel::IOCOOMCoreModel(Core *core)
   : CoreModel(core)
{
   // Initialize instruction fetch unit
   _instruction_fetch_stage = new InstructionFetchStage(this);
   // Initialize instruction decode unit
   _instruction_decode_stage = new InstructionDecodeStage(this);
   // Initialize register fetch stage
   _register_fetch_stage = new RegisterFetchStage(this);
   // Initialize dispatch stage
   _dispatch_stage = new DispatchStage(this);
   // Initialize execution unit
   _execution_unit = new ExecutionUnit();
   // Initialize load/store queues
   _load_store_unit = new LoadStoreUnit(this);

   // For Power and Area Modeling
   UInt32 num_load_queue_entries = 0;
   UInt32 num_store_queue_entries = 0;
   try
   {
      num_load_queue_entries = Sim()->getCfg()->getInt("core/iocoom/num_load_queue_entries");
      num_store_queue_entries = Sim()->getCfg()->getInt("core/iocoom/num_store_queue_entries");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read [core/iocoom] params from the config file");
   }

   // Initialize McPAT
   initializeMcPATInterface(num_load_queue_entries, num_store_queue_entries);
}

IOCOOMCoreModel::~IOCOOMCoreModel()
{
   delete _load_store_unit;
   delete _execution_unit;
   delete _dispatch_stage;
   delete _register_fetch_stage;
   delete _instruction_decode_stage;
   delete _instruction_fetch_stage;
}

void
IOCOOMCoreModel::outputSummary(std::ostream &os, const Time& target_completion_time)
{
   updatePipelineStallCounters(_instruction_fetch_stage->getStallTime(),
                               _dispatch_stage->getMemoryAccessStallTime(),
                               _dispatch_stage->getExecutionUnitStallTime());
   CoreModel::outputSummary(os, target_completion_time);

   os << "    Detailed Stall Time Breakdown (in nanoseconds): " << endl;
   _instruction_fetch_stage->outputSummary(os);
   _dispatch_stage->outputSummary(os);
}

void IOCOOMCoreModel::handleInstruction(Instruction *instruction)
{
   // Execute this first so that instructions have the opportunity to
   // abort further processing (via AbortInstructionException)
   Time cost = instruction->getCost(this);

   if (instruction->isDynamic())
   {
      _curr_time += cost;
      updateDynamicInstructionCounters(instruction, cost);
      return;
   }

   const MicroOpList& micro_op_list = instruction->getMicroOps();

   // Sync with instruction fetch stage
   _curr_time = getMax<Time>(_curr_time, _instruction_fetch_stage->getTimeStamp());

   // Front-end processing of instructions
   // Model instruction fetch stage
   _instruction_fetch_stage->handle(instruction, _curr_time);
   Time instruction_fetch_ready = _instruction_fetch_stage->sync(_instruction_decode_stage->getTimeStamp());

   // Model instruction decode stage
   _instruction_decode_stage->handle(instruction_fetch_ready);
   Time instruction_decode_ready = _instruction_decode_stage->sync(_register_fetch_stage->getTimeStamp());

   // Model register fetch stage
   _register_fetch_stage->handle(instruction_decode_ready);
   Time register_fetch_ready = _register_fetch_stage->sync(_dispatch_stage->getTimeStamp()); 

   // Model dispatch stage
   _dispatch_stage->handle(instruction, register_fetch_ready);

   // Model instruction in the following steps:
   // - find when read operations are available
   // - find latency of instruction
   // - update write operands
 
   Time dispatch_ready = _dispatch_stage->getTimeStamp();
   Time completion_time = dispatch_ready;
   CoreUnit wait_unit = INVALID_UNIT;
 
   // Back-end processing of micro-ops
   for (MicroOpList::const_iterator it = micro_op_list.begin(); it != micro_op_list.end(); it++)
   {
      // Within the micro-ops, always the LOADS are first, EXECUTIONS are next, and STORES are last
      const MicroOp& micro_op = *it;
      dispatch_ready = _dispatch_stage->update(completion_time, wait_unit);

      switch (micro_op.getType())
      {
      case MicroOp::LOAD:
         {
            dispatch_ready = _dispatch_stage->sync(_load_store_unit->getLastLoadAllocateTime(), LOAD_UNIT);
            // An instruction can have two loads that can be issued in parallel
            Time allocate_time = _load_store_unit->allocateLoad(dispatch_ready);
            completion_time = _load_store_unit->issueLoad(allocate_time);
            wait_unit = LOAD_UNIT;
         }
         break;

      case MicroOp::EXEC:
         {
            // Assumption is that execution units have latency but with occupancy of 1
            // Calculate the completion time of instruction (after fetching read operands + execution unit)
            // Assume that there is no structural hazard at the execution unit
            completion_time = _execution_unit->issue(micro_op, dispatch_ready, cost);
            wait_unit = EXECUTION_UNIT;
         }
         break;

      case MicroOp::STORE:
         {
            dispatch_ready = _dispatch_stage->sync(_load_store_unit->getLastStoreAllocateTime(), STORE_UNIT);
            Time allocate_time = _load_store_unit->allocateLoad(dispatch_ready);
            _load_store_unit->issueStore(allocate_time);
         }
         break;

      case MicroOp::LFENCE:
      case MicroOp::SFENCE:
      case MicroOp::MFENCE:
         {
            // Can we make the timestamps corresponding to all entries in the load/store queue 
            // equal to the max of the deallocate time of the last entry?
            _load_store_unit->handleFence(micro_op.getType());
         }
         break;

      default:
         LOG_PRINT_ERROR("Unrecognized micro-op type (%u)", micro_op.getType());
         break;
      }
   }
   

   // If it is a simple load instruction, execute the next instruction after load_queue_ready,
   // else wait till all the operands are fetched to execute the next instruction
   // Just add the cost for dynamic instructions since they involve pipeline stalls
   _dispatch_stage->updateScoreboard(instruction, completion_time, wait_unit);

   // Update memory fence counters
   updateMemoryFenceCounters(instruction);
 
   // Update McPAT counters
   updateMcPATCounters(instruction);
}

IOCOOMCoreModel::InstructionFetchStage::InstructionFetchStage(CoreModel* core_model)
   : _core_model(core_model)
   , _timestamp(0)
   , _total_stall_time(0)
{}

void
IOCOOMCoreModel::InstructionFetchStage::handle(const Instruction* instruction, const Time& curr_time)
{
   assert(_timestamp <= curr_time);

   Time icache_access_time = _core_model->modelICache(instruction);
   _timestamp = (curr_time + icache_access_time);

   if (icache_access_time > ONE_CYCLE)
   {
      Time icache_stall_time = (icache_access_time - ONE_CYCLE);
      _total_stall_time += icache_stall_time;
   }
}

Time
IOCOOMCoreModel::InstructionFetchStage::sync(const Time& next_stage_timestamp)
{
   _timestamp = getMax<Time>(_timestamp, next_stage_timestamp);
   return _timestamp;
}

void
IOCOOMCoreModel::InstructionFetchStage::outputSummary(ostream& os)
{
   // Stall Time
   os << "      Instruction Fetch: " << _total_stall_time.toNanosec() << endl;
}

IOCOOMCoreModel::InstructionDecodeStage::InstructionDecodeStage(CoreModel* core_model)
   : _core_model(core_model)
   , _timestamp(0)
{}

void
IOCOOMCoreModel::InstructionDecodeStage::handle(const Time& fetch_ready)
{
   assert(_timestamp <= fetch_ready);
   _timestamp = (fetch_ready + ONE_CYCLE);
}

Time
IOCOOMCoreModel::InstructionDecodeStage::sync(const Time& next_stage_timestamp)
{
   _timestamp = getMax<Time>(_timestamp, next_stage_timestamp);
   return _timestamp;
}

IOCOOMCoreModel::RegisterFetchStage::RegisterFetchStage(CoreModel* core_model)
   : _core_model(core_model)
   , _timestamp(0)
{
}

void
IOCOOMCoreModel::RegisterFetchStage::handle(const Time& decode_ready)
{
   assert(_timestamp <= decode_ready);
   _timestamp = decode_ready + ONE_CYCLE;
}

Time
IOCOOMCoreModel::RegisterFetchStage::sync(const Time& next_stage_timestamp)
{
   _timestamp = getMax<Time>(_timestamp, next_stage_timestamp);
   return _timestamp;
}

IOCOOMCoreModel::DispatchStage::DispatchStage(CoreModel* core_model)
   : _core_model(core_model)
   , _timestamp(0)
{
   // Initialize register scoreboard
   _register_scoreboard.resize(_NUM_REGISTERS, Time(0));
   _register_dependency_list.resize(_NUM_REGISTERS, INVALID_UNIT); 
   
   initializePipelineStallCounters();
}

void
IOCOOMCoreModel::DispatchStage::outputSummary(ostream& os)
{
   os << "      Load Queue: " << _total_load_queue__stall_time.toNanosec() << endl;
   os << "      Store Queue: " << _total_store_queue__stall_time.toNanosec() << endl;
   os << "      Memory Access (Intra-Instruction): " << _total_intra_ins__memory_access__stall_time.toNanosec() << endl;
   os << "      Memory Access (Inter-Instruction): " << _total_inter_ins__memory_access__stall_time.toNanosec() << endl;
   os << "      Execution Unit (Intra-Instruction): " << _total_intra_ins__execution_unit__stall_time.toNanosec() << endl;
   os << "      Execution Unit (Inter-Instruction): " << _total_inter_ins__execution_unit__stall_time.toNanosec() << endl;
}

Time
IOCOOMCoreModel::DispatchStage::getMemoryAccessStallTime() const
{
   return _total_load_queue__stall_time + _total_store_queue__stall_time + 
          _total_intra_ins__memory_access__stall_time + _total_inter_ins__memory_access__stall_time;
}

Time
IOCOOMCoreModel::DispatchStage::getExecutionUnitStallTime() const
{
   return _total_intra_ins__execution_unit__stall_time + _total_inter_ins__execution_unit__stall_time;
}

void
IOCOOMCoreModel::DispatchStage::initializePipelineStallCounters()
{
   _total_load_queue__stall_time = Time(0);
   _total_store_queue__stall_time = Time(0);
   _total_intra_ins__memory_access__stall_time = Time(0);
   _total_inter_ins__memory_access__stall_time = Time(0);
   _total_intra_ins__execution_unit__stall_time = Time(0);
   _total_inter_ins__execution_unit__stall_time = Time(0);
}

void
IOCOOMCoreModel::DispatchStage::handle(const Instruction* instruction,
                                       const Time& register_fetch_ready)
{
   assert(_timestamp <= register_fetch_ready);
   _timestamp = register_fetch_ready + ONE_CYCLE;
   const RegisterOperandList& read_register_operands = instruction->getReadRegisterOperands();

   // Time when register operands are ready (waiting for either the load unit or the execution unit)
   Time register_operands_ready__load_unit = _timestamp;
   Time register_operands_ready__execution_unit = _timestamp;

   // REGISTER read operands
   for (unsigned int i = 0; i < read_register_operands.size(); i++)
   {
      const RegisterOperand& reg = read_register_operands[i];
      LOG_ASSERT_ERROR(reg < _register_scoreboard.size(), "Register value out of range: %u", reg);

      // Compute the ready time for registers that are waiting on the LOAD_UNIT
      // and on the EXECUTION_UNIT
      // The final ready time is the max of this
      if (_register_dependency_list[reg] == LOAD_UNIT)
      {
         if (register_operands_ready__load_unit < _register_scoreboard[reg])
            register_operands_ready__load_unit = _register_scoreboard[reg];
      }
      else if (_register_dependency_list[reg] == EXECUTION_UNIT)
      {
         if (register_operands_ready__execution_unit < _register_scoreboard[reg])
            register_operands_ready__execution_unit = _register_scoreboard[reg];
      }
      else
      {
         LOG_ASSERT_ERROR(_register_scoreboard[reg] <= _timestamp,
                          "Unrecognized Unit(%u)", _register_dependency_list[reg]);
      }
   }
 
   // Stall time due to memory access and execution unit
   _total_inter_ins__memory_access__stall_time += (register_operands_ready__load_unit - _timestamp);
   _total_inter_ins__execution_unit__stall_time += (register_operands_ready__execution_unit - _timestamp);

   // The read register ready time is the max of this
   _timestamp = getMax<Time>(register_operands_ready__load_unit, register_operands_ready__execution_unit); 
}

Time
IOCOOMCoreModel::DispatchStage::sync(const Time& next_stage_timestamp, const CoreUnit& unit)
{
   if (_timestamp < next_stage_timestamp)
   {
      Time stall_time = next_stage_timestamp - _timestamp;
      if (unit == LOAD_UNIT)
         _total_load_queue__stall_time += stall_time;
      else if (unit == STORE_UNIT)
         _total_store_queue__stall_time += stall_time;
      _timestamp = next_stage_timestamp;
   }
   return _timestamp;
}

Time
IOCOOMCoreModel::DispatchStage::update(const Time& prev_micro_op_completion_time, const CoreUnit& wait_unit)
{
   LOG_ASSERT_ERROR(_timestamp <= prev_micro_op_completion_time,
                    "TimeStamp(%llu ns), Prev-MicroOp-Completion(%llu ns), Wait-Unit(%u)",
                    _timestamp.toNanosec(), prev_micro_op_completion_time.toNanosec(), wait_unit);
   Time stall_time = prev_micro_op_completion_time - _timestamp;
   if (wait_unit == LOAD_UNIT)
      _total_intra_ins__memory_access__stall_time += stall_time;
   else if (wait_unit == EXECUTION_UNIT)
      _total_intra_ins__execution_unit__stall_time += stall_time;
   _timestamp = prev_micro_op_completion_time;
   return _timestamp;
}

void
IOCOOMCoreModel::DispatchStage::updateScoreboard(Instruction* instruction,
                                                 const Time& completion_time, const CoreUnit& wait_unit)
{
   // REGISTER write operands
   // In this core model, we directly resolve WAR hazards since we wait
   // for all the read operands of an instruction to be available before we issue it
   // Assume that the register file can be written in one cycle
   const RegisterOperandList& write_register_operands = instruction->getWriteRegisterOperands();
   for (unsigned int i = 0; i < write_register_operands.size(); i++)
   {
      const RegisterOperand& reg = write_register_operands[i];
      LOG_ASSERT_ERROR(reg < _register_scoreboard.size(), "Register value out of range: %u", reg);
      
      // The only case where this assertion is not true is when the register is written
      // into but is never read before the next write operation. We assume
      // that this never happened
      _register_scoreboard[reg] = completion_time;
      // Update the unit that the register is waiting for
      _register_dependency_list[reg] = wait_unit;
   }
}

Time
IOCOOMCoreModel::ExecutionUnit::issue(const MicroOp& micro_op, const Time& issue_time, const Time& cost)
{
   return (issue_time + cost);
}

IOCOOMCoreModel::LoadStoreUnit::LoadStoreUnit(CoreModel* core_model)
   : _core_model(core_model)
{
   _load_queue = new LoadQueue(core_model);
   _store_queue = new StoreQueue(core_model);
}

IOCOOMCoreModel::LoadStoreUnit::~LoadStoreUnit()
{
   delete _store_queue;
   delete _load_queue;
}

Time
IOCOOMCoreModel::LoadStoreUnit::allocateLoad(const Time& schedule_time)
{
   return _load_queue->allocate(schedule_time);
}

Time
IOCOOMCoreModel::LoadStoreUnit::allocateStore(const Time& schedule_time)
{
   return _store_queue->allocate(schedule_time);
}

Time
IOCOOMCoreModel::LoadStoreUnit::issueLoad(const Time& issue_time)
{
   // Assume memory is read only after all registers are read
   // This may be required since some registers may be used as the address for memory operations
   // Time when load unit and memory operands are ready
   // MEMORY read operands
   const DynamicMemoryInfo &info = _core_model->getDynamicMemoryInfo();
   LOG_ASSERT_ERROR(info._read, "Expected memory read info");

   Time load_completion_time;
   // Check if data is present in store queue, If yes, just bypass
   StoreQueue::Status status = _store_queue->isAddressAvailable(issue_time, info._address);
   if (status == StoreQueue::VALID)
   {
      pair<Time,Time> timing_info = _load_queue->issue(issue_time, true, info);
      load_completion_time = issue_time + ONE_CYCLE;
   }
   else // (status != StoreQueue::VALID)
   {
      pair<Time,Time> timing_info = _load_queue->issue(issue_time, false, info);
      load_completion_time = timing_info.first;
   }

   _core_model->popDynamicMemoryInfo();
   return load_completion_time; 
}

void
IOCOOMCoreModel::LoadStoreUnit::issueStore(const Time& issue_time)
{
   // MEMORY write operands
   // This is done before doing register
   // operands to make sure the scoreboard is updated correctly
   const DynamicMemoryInfo& info = _core_model->getDynamicMemoryInfo();
   LOG_ASSERT_ERROR(!info._read, "Expected memory write info");
   
   // This just updates the contents of the store buffer
   // Find the last load deallocate time
   Time last_load_deallocate_time = _load_queue->getLastDeallocateTime();
   _store_queue->issue(issue_time, last_load_deallocate_time, info);

   _core_model->popDynamicMemoryInfo();
}

void
IOCOOMCoreModel::LoadStoreUnit::handleFence(MicroOp::Type micro_op_type)
{
   Time last_load_deallocate_time = _load_queue->getLastDeallocateTime();
   Time last_store_deallocate_time = _store_queue->getLastDeallocateTime();
   Time last_memop_deallocate_time = getMax<Time>(last_load_deallocate_time, last_store_deallocate_time);
   
   switch (micro_op_type)
   {
   case MicroOp::LFENCE:
      _load_queue->setFenceTime(last_load_deallocate_time);
      break;
   case MicroOp::SFENCE:
      _store_queue->setFenceTime(last_store_deallocate_time);
      break;
   case MicroOp::MFENCE:
      _load_queue->setFenceTime(last_memop_deallocate_time);
      _store_queue->setFenceTime(last_memop_deallocate_time);
      break;
   default:
      LOG_PRINT_ERROR("Unrecognized micro-op type: %u", micro_op_type);
      break;
   }
}

// Load Queue 

IOCOOMCoreModel::LoadStoreUnit::LoadQueue::LoadQueue(CoreModel* core_model)
   : _core_model(core_model)
   , _fence_time(0)
{
   config::Config* cfg = Sim()->getCfg();
   try
   {
      _num_entries = cfg->getInt("core/iocoom/num_load_queue_entries");
      _speculative_loads_enabled = cfg->getBool("core/iocoom/speculative_loads_enabled");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read [core/iocoom] parameters from the cfg file");
   }
   _scoreboard.resize(_num_entries, Time(0));
   _allocate_idx = 0;
}

IOCOOMCoreModel::LoadStoreUnit::LoadQueue::~LoadQueue()
{
}

Time
IOCOOMCoreModel::LoadStoreUnit::LoadQueue::allocate(const Time& schedule_time)
{
   return getMax<Time>(_scoreboard[_allocate_idx] + ONE_CYCLE, schedule_time);
}

pair<Time,Time>
IOCOOMCoreModel::LoadStoreUnit::LoadQueue::issue(const Time& issue_time, bool found_in_store_queue,
                                                 const DynamicMemoryInfo& info)
{
   const Time& load_latency = (found_in_store_queue) ? ONE_CYCLE : info._latency;
   Time actual_issue_time = getMax<Time>(issue_time, _fence_time);

   // Issue loads to cache hierarchy one by one
   Time completion_time;
   Time deallocate_time;
   UInt32 last_idx = (_allocate_idx + _num_entries-1) % (_num_entries);

   if (_speculative_loads_enabled)
   {
      // With speculative loads, issue_time = allocate_time
      completion_time = actual_issue_time + load_latency;
      // The load queue should be de-allocated in order for memory consistency purposes
      // Assumption: Only one load can be deallocated per cycle
      deallocate_time = getMax<Time>(completion_time, _scoreboard[last_idx] + ONE_CYCLE);
   }
   else // (!_speculative_loads_enabled)
   {
      // With non-speculative loads, loads can be issued and completed only in FIFO order
      actual_issue_time = getMax<Time>(actual_issue_time, _scoreboard[last_idx]);
      completion_time = actual_issue_time + load_latency;
      deallocate_time = completion_time;
   }

   _scoreboard[_allocate_idx] = deallocate_time;
   _allocate_idx = (_allocate_idx + 1) % (_num_entries);
   return make_pair(completion_time, deallocate_time);
}

const Time&
IOCOOMCoreModel::LoadStoreUnit::LoadQueue::getLastAllocateTime()
{
   return _scoreboard[_allocate_idx];
}

const Time&
IOCOOMCoreModel::LoadStoreUnit::LoadQueue::getLastDeallocateTime()
{
   UInt32 last_idx = (_allocate_idx + _num_entries-1) % _num_entries;
   return _scoreboard[last_idx];
}

void
IOCOOMCoreModel::LoadStoreUnit::LoadQueue::setFenceTime(const Time& fence_time)
{
   _fence_time = fence_time;
}

// Store Queue

IOCOOMCoreModel::LoadStoreUnit::StoreQueue::StoreQueue(CoreModel* core_model)
   : _core_model(core_model)
   , _fence_time(0)
{
   // The assumption is the store queue is reused as a store buffer
   // Committed stores have an additional "C" flag enabled
   config::Config* cfg = Sim()->getCfg();
   try
   {
      _num_entries = cfg->getInt("core/iocoom/num_store_queue_entries");
      _multiple_outstanding_RFOs_enabled = cfg->getBool("core/iocoom/multiple_outstanding_RFOs_enabled");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read [core/iocoom] params from the cfg file");
   }

   _scoreboard.resize(_num_entries, Time(0));
   _addresses.resize(_num_entries, INVALID_ADDRESS);
   _allocate_idx = 0;
}

IOCOOMCoreModel::LoadStoreUnit::StoreQueue::~StoreQueue()
{
}

Time
IOCOOMCoreModel::LoadStoreUnit::StoreQueue::allocate(const Time& schedule_time)
{
   return getMax<Time>(_scoreboard[_allocate_idx] + ONE_CYCLE, schedule_time);
}

Time
IOCOOMCoreModel::LoadStoreUnit::StoreQueue::issue(const Time& issue_time,
                                                  const Time& last_load_deallocate_time,
                                                  const DynamicMemoryInfo& info)
{
   const Time& store_latency = info._latency;
   const IntPtr& address = info._address;
   Time actual_issue_time = getMax<Time>(issue_time, _fence_time);

   // Note: basically identical to LoadQueue, except we need to track addresses as well.
   // We can't do store buffer coalescing. It violates x86 TSO memory consistency model.
   
   Time deallocate_time;
   
   UInt32 last_idx = (_allocate_idx + _num_entries-1) % (_num_entries);
   const Time& last_store_deallocate_time = _scoreboard[last_idx];
   
   if (_multiple_outstanding_RFOs_enabled)
   {
      // With multiple outstanding RFOs, issue_time = allocate_time
      Time RFO_completion_time = actual_issue_time + store_latency;
      // The store queue should be de-allocated in order for memory consistency purposes
      // Assumption: Only one store can be deallocated per cycle
      deallocate_time = getMax<Time>(RFO_completion_time, last_store_deallocate_time, last_load_deallocate_time) + ONE_CYCLE;
   }
   else // (!_multiple_outstanding_RFOs_enabled)
   {
      // With multiple outstanding RFOs disabled, stores can be issued and completed only in FIFO order
      Time actual_issue_time = getMax<Time>(actual_issue_time, last_store_deallocate_time, last_load_deallocate_time);
      deallocate_time = actual_issue_time + store_latency;
   }

   _scoreboard[_allocate_idx] = deallocate_time;
   _addresses[_allocate_idx] = address;
   _allocate_idx = (_allocate_idx + 1) % (_num_entries);
   return deallocate_time;
}

const Time&
IOCOOMCoreModel::LoadStoreUnit::StoreQueue::getLastAllocateTime()
{
   return _scoreboard[_allocate_idx];
}

const Time&
IOCOOMCoreModel::LoadStoreUnit::StoreQueue::getLastDeallocateTime()
{
   UInt32 last_idx = (_allocate_idx + _num_entries-1) % _num_entries;
   return _scoreboard[last_idx];
}

void
IOCOOMCoreModel::LoadStoreUnit::StoreQueue::setFenceTime(const Time& fence_time)
{
   _fence_time = fence_time;
}

IOCOOMCoreModel::LoadStoreUnit::StoreQueue::Status
IOCOOMCoreModel::LoadStoreUnit::StoreQueue::isAddressAvailable(const Time& schedule_time, IntPtr address)
{
   for (unsigned int i = 0; i < _scoreboard.size(); i++)
   {
      if (_addresses[i] == address)
      {
         if (_scoreboard[i] >= schedule_time)
            return VALID;
      }
   }
   return NOT_FOUND;
}
