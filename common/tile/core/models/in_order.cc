#include "in_order.h"
#include "core.h"
#include "config.hpp"
#include "simulator.h"
#include "branch_predictor.h"
#include "tile.h"
#include "utils.h"
#include "log.h"

InOrderCoreModel::InOrderCoreModel(Core* core)
   : CoreModel(core)
   , _dispatch_time(0)
   , _commit_time(0)
   , _total_branch_speculation_violation__stall_time(0)
   , _total_load_speculation_violation__stall_time(0)
{
   // Initialize instruction fetch unit
   _instruction_fetch_stage = new InstructionFetchStage(this);
   // Initialize execution unit
   _execution_unit = new ExecutionUnit(this);
   // Initialize branch unit
   _branch_unit = new BranchUnit(this);
   // Initialize load/store queues
   _load_queue = new LoadQueue(this);
   _store_queue = new StoreQueue(this);

   // Initialize register scoreboard/dependency tracking
   _register_scoreboard.resize(_NUM_REGISTERS, Time(0));
   _register_dependency_list.resize(_NUM_REGISTERS, MicroOp::INVALID); 
 
   // For Power and Area Modeling
   UInt32 num_load_queue_entries = 0;
   UInt32 num_store_queue_entries = 0;
   try
   {
      num_load_queue_entries = Sim()->getCfg()->getInt("core/in_order/num_load_queue_entries");
      num_store_queue_entries = Sim()->getCfg()->getInt("core/in_order/num_store_queue_entries");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read [core/in_order] params from the config file");
   }

   // Initialize McPAT
   initializeMcPATInterface(num_load_queue_entries, num_store_queue_entries);
}

InOrderCoreModel::~InOrderCoreModel()
{
   delete _store_queue;
   delete _load_queue;
   delete _branch_unit;
   delete _execution_unit;
   delete _instruction_fetch_stage;
}

void
InOrderCoreModel::outputSummary(std::ostream &os, const Time& target_completion_time)
{
   Time memory_stall_time = _total_memory_access__stall_time +
                            _load_queue->getStallTime() + _store_queue->getStallTime();
   updatePipelineStallCounters(_instruction_fetch_stage->getStallTime(),
                               memory_stall_time,
                               _load_queue->getStallTime(), _store_queue->getStallTime(),
                               _total_execution_unit__stall_time,
                               _total_branch_speculation_violation__stall_time,
                               _total_load_speculation_violation__stall_time);
   CoreModel::outputSummary(os, target_completion_time);
}

void
InOrderCoreModel::handleDynamicInstruction(DynamicInstruction* instruction)
{
   // Special handling for dynamic instructions
   _curr_time += instruction->getCost();
   updateDynamicInstructionStallCounters(instruction);
}

void
InOrderCoreModel::handleInstruction(Instruction *instruction)
{
   // Sync with instruction fetch stage
   _curr_time = getMax<Time>(_curr_time, _instruction_fetch_stage->getTimeStamp());

   // Front-end processing of instructions
   // Model instruction fetch stage
   _instruction_fetch_stage->handle(instruction, _curr_time);
   const Time& fetch_ready = _instruction_fetch_stage->sync(_dispatch_time);

   LOG_PRINT("Curr-Time(%llu ns), Fetch-Ready(%llu ns)", _curr_time.toNanosec(), fetch_ready.toNanosec());
   _dispatch_time = fetch_ready;
 
   // Back-end processing of micro-ops
   for (uint32_t i = 0; i < instruction->getNumUops(); i++)
   {
      // Model allocate stage
      // 1) Register allocation (assume physical register file is sized according to reorder buffer, so no stalls)
      // 2) For loads, load-queue allocation
      // 3) For stores, store-queue allocation
      // Within the micro-ops, always the LOADS are first, EXECUTIONS are next, and STORES are last

      const MicroOp& micro_op = instruction->getUop(i);
     
      // Get time when operands are ready
      Time operands_ready_list[2];    // Only 2 operands per MicroOp
      Time all_operands_ready = getOperandsReady(micro_op, operands_ready_list);
      
      // Get issue_time
      if (_dispatch_time < all_operands_ready)
      {
         Time stall_time = all_operands_ready - _dispatch_time;
         _dispatch_time = all_operands_ready;
         // Update stall counters
         updateStallCounters(micro_op, stall_time, operands_ready_list);
      }

      Time results_ready(0);

      // If it is a simple load instruction, execute the next instruction after load_queue_ready,
      // else wait till all the operands are fetched to execute the next instruction
      // Just add the cost for dynamic instructions since they involve pipeline stalls
      switch (micro_op.type)
      {
      case MicroOp::GENERAL:
         // Assumption is that execution units have latency but with occupancy of 1
         // Calculate the completion time of instruction (after fetching read operands + execution unit)
         // Assume that there is no structural hazard at the execution unit
         results_ready = _execution_unit->handle(_dispatch_time, _commit_time, micro_op.lat);
         break;

      case MicroOp::LOAD:
         {
            bool speculation_failed = false;
            results_ready = _load_queue->handle(_dispatch_time, _commit_time,
                                                speculation_failed, _store_queue);
            // Did speculation fail?
            if (speculation_failed)
            {
               _total_load_speculation_violation__stall_time += (_commit_time - _curr_time);
               _curr_time = _commit_time;
            }
         }
         break;

      case MicroOp::STORE:
         _store_queue->handle(_dispatch_time, _commit_time);
         break;

      case MicroOp::STORE_ADDR:
         results_ready = _execution_unit->handle(_dispatch_time, _commit_time, micro_op.lat);
         break;

      case MicroOp::FENCE:
         // Can we make the timestamps corresponding to all entries in the load/store queue
         // equal to the max of the deallocate time of the last entry?
         _store_queue->handleFence(_dispatch_time, _commit_time);
         // Update memory fence counters
         updateMemoryFenceCounters();
         break;

      case MicroOp::BRANCH:
         {
            bool speculation_failed = false;
            results_ready = _branch_unit->handle(_dispatch_time, _commit_time, micro_op.lat,
                                                 instruction->getAddress(), _branch_predictor,
                                                 speculation_failed);
            if (speculation_failed)
            {
               // For the branch stall time, have to wait for the greater of the following
               //  1) Branch misprediction latency
               //  2) Branch operands to be ready
               // The reasoning is that the pipeline backend waits for the branch operands in parallel
               // with the stages responsible for the misprediction latency
               Time mispredict_penalty = getLatency(_branch_predictor->getMispredictPenalty());
               Time branch_stall_time = getMax<Time>(mispredict_penalty, results_ready - _curr_time);
               _total_branch_speculation_violation__stall_time += branch_stall_time;
               _curr_time += branch_stall_time;
            }
         }
         break;

      default:
         LOG_PRINT_ERROR("Unrecognized micro-op type: %u", micro_op.type);
         break;
      }

      LOG_ASSERT_ERROR(_commit_time > _dispatch_time,
                       "Uop-Type(%u): Curr-Time(%llu ns), Fetch-Ready(%llu ns), "
                       "Dispatch-Time(%llu ns), Results-Ready(%llu ns), Commit-Time(%llu ns)",
                       micro_op.type, _curr_time.toNanosec(), fetch_ready.toNanosec(),
                       _dispatch_time.toNanosec(), results_ready.toNanosec(), _commit_time.toNanosec());
      
      // Update register scoreboard
      updateRegisterScoreboard(micro_op, results_ready);
   }
   
   // Update McPAT counters
   updateMcPATCounters(instruction);
}

Time
InOrderCoreModel::getOperandsReady(const MicroOp& micro_op, Time* operands_ready_list) const
{
   // Query Scoreboard to get when operands are ready
   operands_ready_list[0] = micro_op.rs[0] ? _register_scoreboard[micro_op.rs[0]] : Time(0);
   operands_ready_list[1] = micro_op.rs[1] ? _register_scoreboard[micro_op.rs[1]] : Time(0);
   Time all_operands_ready = getMax<Time>(operands_ready_list[0], operands_ready_list[1]);
   LOG_PRINT("SRC: Register-Scoreboard [%u, %llu ns] [%u, %llu ns], Operands-Ready(%llu ns)",
             micro_op.rs[0], operands_ready_list[0].toNanosec(),
             micro_op.rs[1], operands_ready_list[1].toNanosec(),
             all_operands_ready.toNanosec());
   return all_operands_ready;
}

void
InOrderCoreModel::updateStallCounters(const MicroOp& micro_op, const Time& stall_time,
                                      const Time* operands_ready_list)
{
   int op_num = (operands_ready_list[0] > operands_ready_list[1]) ? 0 : 1;
   const MicroOp::Type uop_type = _register_dependency_list[micro_op.rs[op_num]];
   assert(uop_type != MicroOp::INVALID && uop_type != MicroOp::STORE && uop_type != MicroOp::FENCE);
   if (uop_type == MicroOp::GENERAL || uop_type == MicroOp::BRANCH)
      _total_execution_unit__stall_time += stall_time;
   else // MicroOp::LOAD, MicroOp::STORE_ADDR
      _total_memory_access__stall_time += stall_time;
}

void
InOrderCoreModel::updateRegisterScoreboard(const MicroOp& micro_op, const Time& results_ready)
{
   // Update register scoreboard
   if (micro_op.rd[0])
   {
      _register_scoreboard[micro_op.rd[0]] = results_ready;
      _register_dependency_list[micro_op.rd[0]] = micro_op.type;
   }
   if (micro_op.rd[1])
   {
      _register_scoreboard[micro_op.rd[1]] = results_ready;
      _register_dependency_list[micro_op.rd[1]] = micro_op.type;
   }
   LOG_PRINT("DEST: Register-Scoreboard [%u, %llu ns] [%u, %llu ns]",
             micro_op.rd[0], _register_scoreboard[micro_op.rd[0]].toNanosec(),
             micro_op.rd[1], _register_scoreboard[micro_op.rd[1]].toNanosec());
}

// Instruction Fetch
InOrderCoreModel::InstructionFetchStage::InstructionFetchStage(CoreModel* core_model)
   : _core_model(core_model)
   , _timestamp(0)
   , _total_stall_time(0)
{}

void
InOrderCoreModel::InstructionFetchStage::handle(const Instruction* instruction, const Time& start_time)
{
   assert(_timestamp <= start_time);

   Time icache_access_time = _core_model->issueInstructionFetch(start_time, instruction->getAddress(), instruction->getSize());
   _timestamp = (start_time + icache_access_time);

   if (icache_access_time > ONE_CYCLE)
   {
      Time icache_stall_time = (icache_access_time - ONE_CYCLE);
      _total_stall_time += icache_stall_time;
   }
}

const Time&
InOrderCoreModel::InstructionFetchStage::sync(const Time& next_stage_timestamp)
{
   _timestamp = getMax<Time>(_timestamp, next_stage_timestamp);
   return _timestamp;
}

// Execution Unit
InOrderCoreModel::ExecutionUnit::ExecutionUnit(CoreModel* core_model)
   : _core_model(core_model)
{}

Time
InOrderCoreModel::ExecutionUnit::handle(const Time& issue_time, Time& commit_time, uint16_t lat)
{
   Time cost = _core_model->getLatency(lat);
   Time results_ready = issue_time + cost;
   commit_time = getMax<Time>(commit_time, results_ready);
   return results_ready;
}

// Branch Unit
InOrderCoreModel::BranchUnit::BranchUnit(CoreModel* core_model)
   : _core_model(core_model)
{}

Time
InOrderCoreModel::BranchUnit::handle(const Time& issue_time, Time& commit_time, uint16_t lat,
                                     uintptr_t address, BranchPredictor* branch_predictor,
                                     bool& speculation_failed)
{
   Time cost = _core_model->getLatency(lat);
   Time results_ready = issue_time + cost;
   commit_time = getMax<Time>(commit_time, results_ready);
   speculation_failed = branch_predictor->handle(address);
   return results_ready;
}

// Load Queue 
InOrderCoreModel::LoadQueue::LoadQueue(CoreModel* core_model)
   : _core_model(core_model)
   , _ordering_point(0)
   , _total_stall_time(0)
{
   config::Config* cfg = Sim()->getCfg();
   try
   {
      _num_entries = cfg->getInt("core/in_order/num_load_queue_entries");
      _multiple_outstanding_loads_enabled = cfg->getBool("core/in_order/multiple_outstanding_loads_enabled");
      _speculation_support_enabled = cfg->getBool("core/speculation_support/enabled");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read [core/in_order] parameters from the cfg file");
   }

   // Scoreboard
   _scoreboard.resize(_num_entries, Time(0));
   _allocate_idx = 0;

   // Speculation handler
   _load_speculation_handler = LoadSpeculationHandler::create();
}

InOrderCoreModel::LoadQueue::~LoadQueue()
{
}

Time
InOrderCoreModel::LoadQueue::handle(Time& dispatch_time, Time& commit_time,
                                    bool& speculation_failed, const StoreQueue* store_queue)
{
   // Note: There is no throughput restriction on the load queue
   // Get the dynamic memory info
   const DynamicMemoryInfo& info = _core_model->getDynamicMemoryInfo();
   assert(info._mem_op_type == Core::READ || info._mem_op_type == Core::READ_EX);

   LOG_PRINT("Load-Queue: handle[Address(%#lx), Size(%u), Mem-Op-Type(%s), Lock-Signal(%s)]",
             info._address, info._size, SPELL_MEMOP(info._mem_op_type), SPELL_LOCK_SIGNAL(info._lock_signal));

   // Load queue allocation
   Time allocate_time = getMax<Time>(dispatch_time, _scoreboard[_allocate_idx]);
   _total_stall_time += (allocate_time - dispatch_time);
   dispatch_time = allocate_time;

   // Address Translation
   Time address_translation_ready = dispatch_time + ONE_CYCLE;

   Time issue_time;

   Scheme scheme = getScheme();

   switch (scheme)
   {
   case IN_PARALLEL:
      issue_time = address_translation_ready;
      break;

   case SERIALIZE:
      issue_time = getMax<Time>(address_translation_ready, _ordering_point);
      break;

   default:
      LOG_PRINT_ERROR("Unrecognized Scheme: %u", scheme);
      break;
   }

   Time latency = ( store_queue->isAddressAvailable(issue_time, info._address)
                     && (info._lock_signal == Core::NONE) )
                   ? ONE_CYCLE : info._latency;
   
   Time completion_time = issue_time + latency;   
   if (scheme == SERIALIZE)
      _ordering_point = completion_time;

   speculation_failed = _load_speculation_handler->check(latency, info);

   // The load queue should be de-allocated in order for memory consistency purposes
   // Assumption: Only one load can be deallocated per cycle
   commit_time = getMax<Time>(commit_time, completion_time);

   _scoreboard[_allocate_idx] = commit_time;
   _allocate_idx = (_allocate_idx + 1) % (_num_entries);
   
   // Remove the dynamic memory info from the queue
   _core_model->popDynamicMemoryInfo();

   return completion_time;
}

InOrderCoreModel::LoadQueue::Scheme
InOrderCoreModel::LoadQueue::getScheme() const
{
   // -> allocate_time = max(ROB_allocate_ready, entry_deallocate_time)
   // ==> multiple_outstanding_loads_enabled = true
   //    ==> speculation_support_enabled = true
   //       [[ CASE 1 ]]
   //       -> issue_time = dispatch_time
   //       -> completion_time = issue_time + latency
   //    ==> speculation_support_enabled = false
   //       [[ CASE 2 ]]
   //       -> issue_time = max(dispatch_time, ordering_point)
   //       -> completion_time = issue_time + latency
   //       -> ordering_point = completion_time
   // ==> multiple_outstanding_loads_enabled = false
   //    [[ CASE 2 ]]
   // -> commit_time = max(commit_time, completion_time)

   if (_multiple_outstanding_loads_enabled)
      if (_speculation_support_enabled)
         return IN_PARALLEL;
      else // (!_speculation_support_enabled)
         return SERIALIZE;
   else // (!_multiple_outstanding_loads_enabled)
      return SERIALIZE;
}

void
InOrderCoreModel::LoadQueue::outputSummaryLoadSpeculation(ostream& os)
{
   _load_speculation_handler->outputSummary(os);
}

// Store Queue
InOrderCoreModel::StoreQueue::StoreQueue(CoreModel* core_model)
   : _core_model(core_model)
   , _total_stall_time(0)
{
   // The assumption is the store queue is reused as a store buffer
   // Committed stores have an additional "C" flag enabled
   config::Config* cfg = Sim()->getCfg();
   try
   {
      _num_entries = cfg->getInt("core/in_order/num_store_queue_entries");
      _multiple_outstanding_stores_enabled = cfg->getBool("core/in_order/multiple_outstanding_stores_enabled");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read [core/in_order] params from the cfg file");
   }

   // Scoreboarding
   _scoreboard.resize(_num_entries, Time(0));
   _operands_scoreboard.resize(_num_entries, Time(0));
   _addresses.resize(_num_entries, INVALID_ADDRESS);
   _allocate_idx = 0;
}

InOrderCoreModel::StoreQueue::~StoreQueue()
{
}

void
InOrderCoreModel::StoreQueue::handle(Time& dispatch_time, Time& commit_time)
{
   // Get the dynamic memory info
   const DynamicMemoryInfo& info = _core_model->getDynamicMemoryInfo();
   assert(info._mem_op_type == Core::WRITE);
  
   // We can't do store buffer coalescing. It violates x86 TSO memory consistency model.
   // A store fence (SFENCE) does not do anything since all memory accesses obey TSO.
 
   const IntPtr& address = info._address;

   LOG_PRINT("Store-Queue: handle[Address(%#lx), Size(%u), Mem-Op-Type(%s), Lock-Signal(%s)]",
             address, info._size, SPELL_MEMOP(info._mem_op_type), SPELL_LOCK_SIGNAL(info._lock_signal));

   // Store queue allocation
   Time allocate_time = getMax<Time>(dispatch_time, _scoreboard[_allocate_idx]);
   _total_stall_time += (allocate_time - dispatch_time);
   dispatch_time = allocate_time;
   
   // Address Translation
   Time address_translation_ready = dispatch_time + ONE_CYCLE;

   commit_time = getMax<Time>(commit_time, address_translation_ready);
   const Time& last_deallocate_time = getLastDeallocateTime();

   Time completion_time; // Time at which store is completed (removed from store buffer)

   Scheme scheme = getScheme();

   switch (scheme)
   {
   case TWO_PHASES:
      {
         // 1st Phase: Exclusive store prefetch (RFO)
         // 2nd Phase: Actual store takes place
         Time RFO_latency = (info._lock_signal == Core::NONE) ? info._latency : Time(0);
         // Phase-2 latency is usually the access time of the L1-D cache (assuming a hit)
         Time store_latency = ONE_CYCLE; // _L1_D_cache_latency;

         Time RFO_issue_time = address_translation_ready;
         Time RFO_completion_time = RFO_issue_time + RFO_latency;
         Time store_issue_time = getMax<Time>(RFO_completion_time, commit_time, last_deallocate_time);
         completion_time = store_issue_time + store_latency;
         
         LOG_PRINT("TWO_PHASES: RFO: Issue-Time(%llu ns), Latency(%llu ns), Completion-Time(%llu ns)",
                   RFO_issue_time.toNanosec(), RFO_latency.toNanosec(), RFO_completion_time.toNanosec());
         LOG_PRINT("TWO_PHASES: STORE: Issue-Time(%llu ns), Latency(%llu ns), Completion-Time(%llu ns)",
                   store_issue_time.toNanosec(), store_latency.toNanosec(), completion_time.toNanosec());
      }
      break;

   case SERIALIZE:
      {
         Time issue_time = getMax<Time>(commit_time, last_deallocate_time);
         completion_time = issue_time + info._latency;
         LOG_PRINT("SERIALIZE: Issue-Time(%llu ns), Latency(%llu ns), Completion-Time(%llu ns)",
                   issue_time.toNanosec(), info._latency.toNanosec(), completion_time.toNanosec());
      }
      break;

   default:
      LOG_PRINT_ERROR("Unrecognized scheme: %u", scheme);
      break;
   }

   Time deallocate_time = completion_time + ONE_CYCLE;
   assert(deallocate_time > last_deallocate_time);

   // The store queue should be de-allocated in order for memory consistency purposes
   assert(address_translation_ready < deallocate_time);
   _operands_scoreboard[_allocate_idx] = dispatch_time;
   _scoreboard[_allocate_idx] = deallocate_time;
   _addresses[_allocate_idx] = address;
   _allocate_idx = (_allocate_idx + 1) % (_num_entries);

   // Remove the dynamic memory info
   _core_model->popDynamicMemoryInfo();
}

void
InOrderCoreModel::StoreQueue::handleFence(Time& dispatch_time, Time& commit_time)
{
   // Store queue allocation
   Time allocate_time = getMax<Time>(dispatch_time, _scoreboard[_allocate_idx]);
   _total_stall_time += (allocate_time - dispatch_time);
   dispatch_time = allocate_time;
   
   Time completion_time = getMax<Time>(dispatch_time, getLastDeallocateTime()) + ONE_CYCLE;
   commit_time = getMax<Time>(commit_time, completion_time);
   
   // Update the scoreboards
   _operands_scoreboard[_allocate_idx] = Time(0);
   _scoreboard[_allocate_idx] = commit_time;
   _addresses[_allocate_idx] = INVALID_ADDRESS;
  
   _allocate_idx = (_allocate_idx + 1) % (_num_entries);
}

const Time&
InOrderCoreModel::StoreQueue::getLastDeallocateTime()
{
   UInt32 last_idx = (_allocate_idx + _num_entries-1) % _num_entries;
   return _scoreboard[last_idx];
}

InOrderCoreModel::StoreQueue::Scheme
InOrderCoreModel::StoreQueue::getScheme() const
{
   // allocate_time = max(ROB_allocate_ready, entry_deallocate_time)
   // commit_time = max(commit_time, dispatch_time)
   // -> multiple_outstanding_stores_enabled = true
   //    -> [[ CASE 1 ]] : assert(info._cacheable)
   //    -> phase_one_issue_time = dispatch_time
   //    -> phase_one_completion_time = phase_one_issue_time + phase_one_latency
   //    -> phase_two_issue_time = max(phase_one_completion_time, commit_time, ordering_point)
   //    -> phase_two_completion_time = phase_two_issue_time + phase_two_latency
   //    -> ordering_point = (info._cacheable) ? phase_two_completion_time : phase_two_issue_time
   //    -> deallocate_time = max(phase_two_completion_time, last_deallocate_time) + ONE_CYCLE
   // -> multiple_outstanding_stores_enabled = false
   //    -> [[ CASE 2 ]]
   //    -> issue_time = max(commit_time, last_deallocate_time)
   //    -> completion_time = issue_time + latency
   //    -> deallocate_time = completion_time + ONE_CYCLE
   
   if (_multiple_outstanding_stores_enabled)
      return TWO_PHASES;
   else // (!_multiple_outstanding_stores_enabled)
      return SERIALIZE;
}

bool
InOrderCoreModel::StoreQueue::isAddressAvailable(const Time& issue_time, IntPtr address) const
{
   for (unsigned int i = 0; i < _scoreboard.size(); i++)
   {
      if (_addresses[i] == address && issue_time >= _operands_scoreboard[i] && issue_time <= _scoreboard[i])
            return true;
   }
   return false;
}
