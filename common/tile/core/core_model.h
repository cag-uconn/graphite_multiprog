#pragma once
// This class represents the actual performance model for a given core

#include <boost/circular_buffer.hpp>
#include <vector>
using std::vector;

// Forward Decls
class Core;
class BranchPredictor;
class McPATCoreInterface;

#include "instruction.h"
#include "basic_block.h"
#include "fixed_types.h"
#include "dynamic_memory_info.h"
#include "dynamic_branch_info.h"

#define ONE_CYCLE    (_core_model->get_ONE_CYCLE())

class CoreModel
{
public:
   CoreModel(Core* core);
   virtual ~CoreModel();

   void processDynamicInstruction(DynamicInstruction* i);
   void queueInstruction(Instruction* instruction);
   void iterate();

   void setDVFS(double old_frequency, double new_voltage, double new_frequency, const Time& curr_time);
   void recomputeAverageFrequency(double frequency); 

   Time getCurrTime() const { return _curr_time; }
   void setCurrTime(Time time);

   void pushDynamicMemoryInfo(const DynamicMemoryInfo &info);
   void popDynamicMemoryInfo();
   const DynamicMemoryInfo& getDynamicMemoryInfo();

   void pushDynamicBranchInfo(const DynamicBranchInfo &info);
   void popDynamicBranchInfo();
   const DynamicBranchInfo& getDynamicBranchInfo();

   static CoreModel* create(Core* core);

   BranchPredictor *getBranchPredictor() { return _bp; }

   void enable();
   void disable();
   bool isEnabled() const { return _enabled; }

   virtual void outputSummary(std::ostream &os, const Time& target_completion_time) = 0;

   void computeEnergy(const Time& curr_time);
   double getDynamicEnergy();
   double getLeakageEnergy();

   class AbortInstructionException {};

   const Time& getCost(InstructionType type) const;

   Core* getCore() { return _core; };

   // Model L1-Instruction Cache
   Time modelICache(const Instruction* instruction);

   const Time& get_ONE_CYCLE() { return _ONE_CYCLE; }

protected:
   enum RegType
   {
      INTEGER = 0,
      FLOATING_POINT
   };
   enum AccessType
   {
      READ = 0,
      WRITE
   };
   enum ExecutionUnitType
   {
   };

   friend class SpawnInstruction;

   typedef boost::circular_buffer<DynamicMemoryInfo> DynamicMemoryInfoQueue;
   typedef boost::circular_buffer<DynamicBranchInfo> DynamicBranchInfoQueue;
   typedef boost::circular_buffer<Instruction*> InstructionQueue;

   Core* _core;

   Time _curr_time;
   
   // 1 Cycle
   Time _ONE_CYCLE;

   void updateMemoryFenceCounters(const Instruction* instruction);
   void updateDynamicInstructionCounters(const Instruction* instruction, const Time& cost);
   void updatePipelineStallCounters(const Time& instruction_fetch__stall_time,
                                    const Time& memory_access__stall_time,
                                    const Time& execution_unit__stall_time);

   // Power/Area modeling
   void initializeMcPATInterface(UInt32 num_load_buffer_entries, UInt32 num_store_buffer_entries);
   void updateMcPATCounters(Instruction* instruction);

private:
   UInt64 _instruction_count;
   double _average_frequency;
   Time _total_time;
   Time _checkpointed_time;
   UInt64 _total_cycles;

   BranchPredictor *_bp;

   InstructionQueue _instruction_queue;
   DynamicMemoryInfoQueue _dynamic_memory_info_queue;
   DynamicBranchInfoQueue _dynamic_branch_info_queue;

   bool _enabled;

   // Instruction costs
   typedef vector<Time> InstructionCosts;
   typedef vector<UInt32> StaticInstructionCosts;
   InstructionCosts _instruction_costs;
   StaticInstructionCosts _static_instruction_costs;

   // Memory fence counters
   UInt64 _total_lfence_instructions;
   UInt64 _total_sfence_instructions;
   UInt64 _total_explicit_mfence_instructions;
   UInt64 _total_implicit_mfence_instructions;
   // Dynamic instruction counters
   UInt64 _total_recv_instructions;
   UInt64 _total_sync_instructions;
   Time _total_recv_instruction__stall_time;
   Time _total_sync_instruction__stall_time;
   // Pipeline stall counters
   Time _total_instruction_fetch__stall_time;
   Time _total_memory_access__stall_time;
   Time _total_execution_unit__stall_time;

   // Power/Area modeling
   McPATCoreInterface* _mcpat_core_interface;
  
   // Main instruction handling function
   virtual void handleInstruction(Instruction* instruction) = 0;

   void __handleInstruction(Instruction* instruction); 
   
   // Instruction costs
   void initializeInstructionCosts(double frequency);
   void updateInstructionCosts(double frequency);

   // Memory fence / Dynamic instruction / Pipeline stall counters
   void initializeMemoryFenceCounters();
   void initializeDynamicInstructionCounters();
   void initializePipelineStallCounters();
};
