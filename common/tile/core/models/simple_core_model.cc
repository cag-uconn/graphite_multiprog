#include "core.h"
#include "log.h"
#include "simple_core_model.h"
#include "branch_predictor.h"
#include "tile.h"

using std::endl;

SimpleCoreModel::SimpleCoreModel(Core *core)
    : CoreModel(core)
{
   // Power/Area modeling
   initializeMcPATInterface(1,1);
}

SimpleCoreModel::~SimpleCoreModel()
{}

void SimpleCoreModel::outputSummary(std::ostream &os, const Time& target_completion_time)
{
   CoreModel::outputSummary(os, target_completion_time);
}

void SimpleCoreModel::handleInstruction(Instruction *instruction)
{
   // Execute this first so that instructions have the opportunity to
   // abort further processing (via AbortInstructionException)
   Time cost = instruction->getCost(this);

   // Update Statistics
   _instruction_count++;

   if (instruction->isDynamic())
   {
      _curr_time += cost;
      updateDynamicInstructionCounters(instruction, cost);
      return;
   }

   Time l1_icache_stall_time(0);
   Time l1_dcache_stall_time(0);
   Time execution_unit_stall_time(0);

   // Instruction Memory Modeling
   Time instruction_memory_access_latency = modelICache(instruction);
   l1_icache_stall_time += instruction_memory_access_latency;

   const UInt32& num_read_memory_operands = instruction->getNumReadMemoryOperands();
   const UInt32& num_write_memory_operands = instruction->getNumWriteMemoryOperands();
   
   for (UInt32 i = 0; i < num_read_memory_operands; i++)
   {
      const DynamicMemoryInfo& info = getDynamicMemoryInfo();
      LOG_ASSERT_ERROR(info._read, "Expected memory read info");

      Time read_latency = info._latency;
      l1_dcache_stall_time += read_latency;
      
      popDynamicMemoryInfo();
   }
   for (UInt32 i = 0; i < num_write_memory_operands; i++)
   {
      const DynamicMemoryInfo& info = getDynamicMemoryInfo();
      LOG_ASSERT_ERROR(!info._read, "Expected memory write info");

      Time write_latency = info._latency;
      l1_dcache_stall_time += write_latency;
      
      popDynamicMemoryInfo();
   }

   execution_unit_stall_time += cost;
   
   _curr_time += (l1_icache_stall_time + l1_dcache_stall_time + execution_unit_stall_time);

   // Update memory fence / pipeline stall counters
   updateMemoryFenceCounters(instruction);
   updatePipelineStallCounters(l1_icache_stall_time, l1_dcache_stall_time, execution_unit_stall_time);

   // Power/Area modeling
   updateMcPATCounters(instruction);
}
