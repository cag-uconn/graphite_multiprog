#include "ooo_core_model.h"
#include "core.h"
#include "config.hpp"
#include "simulator.h"
#include "branch_predictor.h"
#include "tile.h"
#include "utils.h"
#include "log.h"

// FIXME: Solve address aliasing problem

OOOCoreModel::OOOCoreModel(Core *core)
   : CoreModel(core)
{
   // Initialize instruction fetch unit
   _instruction_fetch_stage = new InstructionFetchStage(this);
   // Initialize instruction decode unit
   _instruction_decode_stage = new InstructionDecodeStage(this);
   // Initialize register rename stage
   _register_rename_stage = new RegisterRenameStage(this);
   // Initialize allocate stage
   _allocate_stage = new AllocateStage(this);
   // Initialize reorder buffer
   _reorder_buffer = new ReorderBuffer(this);
   // Initialize execution unit
   _execution_unit = new ExecutionUnit();
   // Initialize load/store queues
   _load_store_unit = new LoadStoreUnit(this);
 
   // Initialize register scoreboard
   _register_scoreboard.resize(_NUM_REGISTERS, Time(0));
 
   // For Power and Area Modeling
   __attribute__((unused)) UInt32 num_reorder_buffer_entries = 0;
   UInt32 num_load_queue_entries = 0;
   UInt32 num_store_queue_entries = 0;
   try
   {
      num_reorder_buffer_entries = Sim()->getCfg()->getInt("core/ooo/num_reorder_buffer_entries");
      num_load_queue_entries = Sim()->getCfg()->getInt("core/ooo/num_load_queue_entries");
      num_store_queue_entries = Sim()->getCfg()->getInt("core/ooo/num_store_queue_entries");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read [core/ooo] params from the config file");
   }

   // Initialize McPAT
   initializeMcPATInterface(num_load_queue_entries, num_store_queue_entries/*, num_reorder_buffer_entries */);
}

OOOCoreModel::~OOOCoreModel()
{
   delete _load_store_unit;
   delete _execution_unit;
   delete _reorder_buffer;
   delete _allocate_stage;
   delete _register_rename_stage;
   delete _instruction_decode_stage;
   delete _instruction_fetch_stage;
}

void
OOOCoreModel::outputSummary(std::ostream &os, const Time& target_completion_time)
{
   CoreModel::outputSummary(os, target_completion_time);

   os << "    Detailed Stall Time Breakdown (in nanoseconds): " << endl;
   _instruction_fetch_stage->outputSummary(os);
   _allocate_stage->outputSummary(os);
   _reorder_buffer->outputSummary(os);
   _load_store_unit->outputSummary(os);
}

void
OOOCoreModel::handleInstruction(Instruction *instruction)
{
   // Execute this first so that instructions have the opportunity to
   // abort further processing (via AbortInstructionException)
   Time cost = instruction->getCost(this);
   
   // Special handling for dynamic instructions
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
   Time fetch_ready = _instruction_fetch_stage->sync(_instruction_decode_stage->getTimeStamp());

   // Model instruction decode stage
   _instruction_decode_stage->handle(fetch_ready);
   Time decode_ready = _instruction_decode_stage->sync(_register_rename_stage->getTimeStamp());

   // Model register rename stage
   _register_rename_stage->handle(decode_ready);
   Time rename_ready = _register_rename_stage->sync(_allocate_stage->getTimeStamp());

   // Check when source registers will be ready
   // REGISTER read operands
   const RegisterOperandList& read_register_operands = instruction->getReadRegisterOperands();
   Time register_operands_ready = rename_ready;
   for (unsigned int i = 0; i < read_register_operands.size(); i++)
   {
      const RegisterOperand& reg = read_register_operands[i];
      LOG_ASSERT_ERROR(reg < _register_scoreboard.size(), "Register value out of range: %u", reg);

      // Compute the ready time for registers that are waiting
      // on the LOAD_STORE_UNIT and the EXECUTION_UNIT
      // The final ready time is the max of this
      if (register_operands_ready < _register_scoreboard[reg])
         register_operands_ready = _register_scoreboard[reg];
   }

   Time allocate_ready = rename_ready + _ONE_CYCLE;
   Time load_completion_time = register_operands_ready;
   Time exec_completion_time = register_operands_ready;
 
   // Back-end processing of micro-ops
   for (MicroOpList::const_iterator it = micro_op_list.begin(); it != micro_op_list.end(); it++)
   {
      // Within the micro-ops, always the LOADS are first, EXECUTIONS are next, and STORES are last
      const MicroOp& micro_op = *it;
      // Model reorder buffer on micro-op allocation
      Time ROB_allocate_time = _reorder_buffer->allocate(micro_op, allocate_ready);
      allocate_ready = getMax<Time>(allocate_ready, ROB_allocate_time);

      switch (micro_op.getType())
      {
      case MicroOp::LOAD:
         {
            Time load_allocate_time = _load_store_unit->allocateLoad(ROB_allocate_time);
            Time load_issue_time = getMax<Time>(load_allocate_time, register_operands_ready);
            pair<Time,Time> timing_info = _load_store_unit->issueLoad(load_issue_time);
            // An instruction can have two loads that can be issued in parallel
            load_completion_time = getMax<Time>(load_completion_time, timing_info.first);
            Time load_deallocate_time = timing_info.second;

            // Model reorder buffer after micro-op completion
            allocate_ready = getMax<Time>(allocate_ready, load_allocate_time);
            _reorder_buffer->complete(micro_op, load_deallocate_time);
         }
         break;

      case MicroOp::EXEC:
         {
            Time exec_issue_time = getMax<Time>(ROB_allocate_time, load_completion_time);
            exec_completion_time = _execution_unit->issue(micro_op, exec_issue_time, cost);

            // Model reorder buffer after micro-op completion
            _reorder_buffer->complete(micro_op, exec_completion_time);
         }
         break;

      case MicroOp::STORE:
         {
            Time store_allocate_time = _load_store_unit->allocateStore(ROB_allocate_time);
            Time store_issue_time = getMax<Time>(store_allocate_time, exec_completion_time);
            __attribute__((unused)) Time store_deallocate_time = _load_store_unit->issueStore(store_issue_time);

            // Model reorder buffer after micro-op completion
            allocate_ready = getMax<Time>(allocate_ready, store_allocate_time);
            // Stores can be retired when they are in the store buffer
            _reorder_buffer->complete(micro_op, store_issue_time);
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

   // Model allocate stage
   assert(rename_ready < allocate_ready);
   Time allocate_latency = allocate_ready - rename_ready;
   _allocate_stage->handle(rename_ready, allocate_latency);

   // REGISTER write operands
   // In this core model, we directly resolve WAR hazards since we wait
   // for all the read operands of an instruction to be available before we issue it
   // Assume that the register file can be written in one cycle
   const RegisterOperandList& write_register_operands = instruction->getWriteRegisterOperands();
   const Time write_operands_ready = getMax<Time>(load_completion_time, exec_completion_time);
   for (unsigned int i = 0; i < write_register_operands.size(); i++)
   {
      const RegisterOperand& reg = write_register_operands[i];
      LOG_ASSERT_ERROR(reg < _register_scoreboard.size(), "Register value out of range: %u", reg);
      
      // The only case where this assertion is not true is when the register is written
      // into but is never read before the next write operation. We assume
      // that this never happened
      _register_scoreboard[reg] = write_operands_ready;
   }

   // Update memory fence counters
   updateMemoryFenceCounters(instruction);
   
   // Update McPAT counters
   updateMcPATCounters(instruction);
}

OOOCoreModel::InstructionFetchStage::InstructionFetchStage(CoreModel* core_model)
   : _core_model(core_model)
   , _timestamp(0)
   , _total_stall_time(0)
{}

void
OOOCoreModel::InstructionFetchStage::handle(const Instruction* instruction, const Time& curr_time)
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
OOOCoreModel::InstructionFetchStage::sync(const Time& next_stage_timestamp)
{
   _timestamp = getMax<Time>(_timestamp, next_stage_timestamp);
   return _timestamp;
}

void
OOOCoreModel::InstructionFetchStage::outputSummary(ostream& os)
{
   // Stall Time
   os << "      Instruction Fetch: " << _total_stall_time.toNanosec() << endl;
}

OOOCoreModel::InstructionDecodeStage::InstructionDecodeStage(CoreModel* core_model)
   : _core_model(core_model)
   , _timestamp(0)
{}

void
OOOCoreModel::InstructionDecodeStage::handle(const Time& fetch_ready)
{
   assert(_timestamp <= fetch_ready);
   _timestamp = (fetch_ready + ONE_CYCLE);
}

Time
OOOCoreModel::InstructionDecodeStage::sync(const Time& next_stage_timestamp)
{
   _timestamp = getMax<Time>(_timestamp, next_stage_timestamp);
   return _timestamp;
}

OOOCoreModel::RegisterRenameStage::RegisterRenameStage(CoreModel* core_model)
   : _core_model(core_model)
   , _timestamp(0)
{
}

void
OOOCoreModel::RegisterRenameStage::handle(const Time& decode_ready)
{
   assert(_timestamp <= decode_ready);
   _timestamp = (decode_ready + ONE_CYCLE);
}

Time
OOOCoreModel::RegisterRenameStage::sync(const Time& next_stage_timestamp)
{
   _timestamp = getMax<Time>(_timestamp, next_stage_timestamp);
   return _timestamp;
}

OOOCoreModel::AllocateStage::AllocateStage(CoreModel* core_model)
   : _core_model(core_model)
   , _timestamp(0)
   , _total_stall_time(0)
{
}

void
OOOCoreModel::AllocateStage::handle(const Time& rename_ready, const Time& allocate_latency)
{
   assert(_timestamp <= rename_ready);
   _timestamp = (rename_ready + allocate_latency);

   assert(allocate_latency >= ONE_CYCLE);
   Time allocate_stall_time = (allocate_latency - ONE_CYCLE);
   _total_stall_time += allocate_stall_time;
}

Time
OOOCoreModel::AllocateStage::sync(const Time& next_stage_timestamp)
{
   _timestamp = getMax<Time>(_timestamp, next_stage_timestamp);
   return _timestamp;
}

void
OOOCoreModel::AllocateStage::outputSummary(ostream& os)
{
   os << "      Allocate Stage: " << _total_stall_time.toNanosec() << endl;
}

OOOCoreModel::ReorderBuffer::ReorderBuffer(CoreModel* core_model)
   : _core_model(core_model)
{
   config::Config* cfg = Sim()->getCfg();
   try
   {
      _num_entries = cfg->getInt("core/ooo/num_reorder_buffer_entries");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read [core/ooo] params from the config file");
   }
   _scoreboard.resize(_num_entries, Time(0));
   _allocate_idx = 0;
   _total_stall_time = Time(0);
}

OOOCoreModel::ReorderBuffer::~ReorderBuffer()
{
}

Time
OOOCoreModel::ReorderBuffer::allocate(const MicroOp& micro_op, const Time& schedule_time)
{
   Time allocate_time = getMax<Time>(_scoreboard[_allocate_idx], schedule_time);
   _total_stall_time += (allocate_time - schedule_time);
   return allocate_time;
}

void
OOOCoreModel::ReorderBuffer::complete(const MicroOp& micro_op, const Time& completion_time)
{
   UInt32 last_idx = (_allocate_idx + _num_entries-1) % _num_entries;
   Time commit_time = getMax<Time>(completion_time, _scoreboard[last_idx]);
   _scoreboard[_allocate_idx] = commit_time;
   _allocate_idx = (_allocate_idx + 1) % _num_entries;
}

void
OOOCoreModel::ReorderBuffer::outputSummary(ostream& os)
{
   // Stall Time
   os << "      Reorder Buffer: " << _total_stall_time.toNanosec() << endl;
}

Time
OOOCoreModel::ExecutionUnit::issue(const MicroOp& micro_op, const Time& issue_time, const Time& cost)
{
   return (issue_time + cost);
}

OOOCoreModel::LoadStoreUnit::LoadStoreUnit(CoreModel* core_model)
   : _core_model(core_model)
{
   _load_queue = new LoadQueue(core_model);
   _store_queue = new StoreQueue(core_model);
   _total_load_queue__stall_time = Time(0);
   _total_store_queue__stall_time = Time(0);
}

OOOCoreModel::LoadStoreUnit::~LoadStoreUnit()
{
   delete _store_queue;
   delete _load_queue;
}

void
OOOCoreModel::LoadStoreUnit::outputSummary(ostream& os)
{
   // Stall Time
   os << "      Load Queue: " << _total_load_queue__stall_time.toNanosec() << endl;
   os << "      Store Queue: " << _total_store_queue__stall_time.toNanosec() << endl;
}

Time
OOOCoreModel::LoadStoreUnit::allocateLoad(const Time& schedule_time)
{
   Time load_allocate_time = _load_queue->allocate(schedule_time);
   _total_load_queue__stall_time += (load_allocate_time - schedule_time);
   return load_allocate_time;
}

Time
OOOCoreModel::LoadStoreUnit::allocateStore(const Time& schedule_time)
{
   Time store_allocate_time = _store_queue->allocate(schedule_time);
   _total_store_queue__stall_time += (store_allocate_time - schedule_time);
   return store_allocate_time;
}

pair<Time,Time>
OOOCoreModel::LoadStoreUnit::issueLoad(const Time& issue_time)
{
   // Assume memory is read only after all registers are read
   // This may be required since some registers may be used as the address for memory operations
   // Time when load unit and memory operands are ready
   // MEMORY read operands
   const DynamicMemoryInfo &info = _core_model->getDynamicMemoryInfo();
   LOG_ASSERT_ERROR(info._read, "Expected memory read info");

   Time load_completion_time;
   Time load_deallocate_time;
   // Check if data is present in store queue, If yes, just bypass
   StoreQueue::Status status = _store_queue->isAddressAvailable(issue_time, info._address);
   if (status == StoreQueue::VALID)
   {
      pair<Time,Time> timing_info = _load_queue->issue(issue_time, true, info);
      load_completion_time = issue_time + ONE_CYCLE;
      load_deallocate_time = timing_info.second;
   }
   else // (status != StoreQueue::VALID)
   {
      pair<Time,Time> timing_info = _load_queue->issue(issue_time, false, info);
      load_completion_time = timing_info.first;
      load_deallocate_time = timing_info.second;
   }

   _core_model->popDynamicMemoryInfo();
   return make_pair(load_completion_time, load_deallocate_time); 
}

Time
OOOCoreModel::LoadStoreUnit::issueStore(const Time& issue_time)
{
   // MEMORY write operands
   // This is done before doing register
   // operands to make sure the scoreboard is updated correctly
   const DynamicMemoryInfo& info = _core_model->getDynamicMemoryInfo();
   LOG_ASSERT_ERROR(!info._read, "Expected memory write info");
   
   // This just updates the contents of the store buffer
   // Find the last load deallocate time
   Time last_load_deallocate_time = _load_queue->getLastDeallocateTime();
   Time store_deallocate_time = _store_queue->issue(issue_time, last_load_deallocate_time, info);

   _core_model->popDynamicMemoryInfo();
   return store_deallocate_time;
}

void
OOOCoreModel::LoadStoreUnit::handleFence(MicroOp::Type micro_op_type)
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

OOOCoreModel::LoadStoreUnit::LoadQueue::LoadQueue(CoreModel* core_model)
   : _core_model(core_model)
   , _fence_time(0)
{
   config::Config* cfg = Sim()->getCfg();
   try
   {
      _num_entries = cfg->getInt("core/ooo/num_load_queue_entries");
      _speculative_loads_enabled = cfg->getBool("core/ooo/speculative_loads_enabled");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read [core/ooo] parameters from the cfg file");
   }
   _scoreboard.resize(_num_entries, Time(0));
   _allocate_idx = 0;
}

OOOCoreModel::LoadStoreUnit::LoadQueue::~LoadQueue()
{
}

Time
OOOCoreModel::LoadStoreUnit::LoadQueue::allocate(const Time& schedule_time)
{
   return getMax<Time>(_scoreboard[_allocate_idx] + ONE_CYCLE, schedule_time);
}

pair<Time,Time>
OOOCoreModel::LoadStoreUnit::LoadQueue::issue(const Time& issue_time, bool found_in_store_queue,
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
OOOCoreModel::LoadStoreUnit::LoadQueue::getLastDeallocateTime()
{
   UInt32 last_idx = (_allocate_idx + _num_entries-1) % _num_entries;
   return _scoreboard[last_idx];
}

void
OOOCoreModel::LoadStoreUnit::LoadQueue::setFenceTime(const Time& fence_time)
{
   _fence_time = fence_time;
}

// Store Queue

OOOCoreModel::LoadStoreUnit::StoreQueue::StoreQueue(CoreModel* core_model)
   : _core_model(core_model)
   , _fence_time(0)
{
   // The assumption is the store queue is reused as a store buffer
   // Committed stores have an additional "C" flag enabled
   config::Config* cfg = Sim()->getCfg();
   try
   {
      _num_entries = cfg->getInt("core/ooo/num_store_queue_entries");
      _multiple_outstanding_RFOs_enabled = cfg->getBool("core/ooo/multiple_outstanding_RFOs_enabled");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read [core/ooo] params from the cfg file");
   }

   _scoreboard.resize(_num_entries, Time(0));
   _addresses.resize(_num_entries, INVALID_ADDRESS);
   _allocate_idx = 0;
}

OOOCoreModel::LoadStoreUnit::StoreQueue::~StoreQueue()
{
}

Time
OOOCoreModel::LoadStoreUnit::StoreQueue::allocate(const Time& schedule_time)
{
   return getMax<Time>(_scoreboard[_allocate_idx] + ONE_CYCLE, schedule_time);
}

Time
OOOCoreModel::LoadStoreUnit::StoreQueue::issue(const Time& issue_time,
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
OOOCoreModel::LoadStoreUnit::StoreQueue::getLastDeallocateTime()
{
   UInt32 last_idx = (_allocate_idx + _num_entries-1) % _num_entries;
   return _scoreboard[last_idx];
}

void
OOOCoreModel::LoadStoreUnit::StoreQueue::setFenceTime(const Time& fence_time)
{
   _fence_time = fence_time;
}

OOOCoreModel::LoadStoreUnit::StoreQueue::Status
OOOCoreModel::LoadStoreUnit::StoreQueue::isAddressAvailable(const Time& schedule_time, IntPtr address)
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
