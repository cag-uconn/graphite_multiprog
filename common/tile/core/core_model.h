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

   void processDynamicInstruction(DynamicInstruction* ins);
   void queueInstruction(Instruction* ins);
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

   void enable();
   void disable();
   bool isEnabled() const { return _enabled; }

   virtual void outputSummary(std::ostream &os, const Time& target_completion_time) = 0;

   void computeEnergy(const Time& curr_time);
   double getDynamicEnergy();
   double getLeakageEnergy();

   Core* getCore() { return _core; };

   // Model instruction fetch
   Time issueInstructionFetch(const Time& issue_time, uintptr_t address, uint32_t size);

   Time getLatency(uint16_t lat) const;
   const Time& get_ONE_CYCLE() const   { return _ONE_CYCLE; }

protected:
   static const UInt32 _NUM_REGISTERS = 512;
   
   Core* _core;
   BranchPredictor* _branch_predictor;

   Time _curr_time;
   
   void updateMemoryFenceCounters();
   void updateDynamicInstructionStallCounters(const DynamicInstruction* ins);
   void updatePipelineStallCounters(const Time& instruction_fetch__stall_time,
                                    const Time& memory_access__stall_time,
                                    const Time& load_queue__stall_time,
                                    const Time& store_queue__stall_time,
                                    const Time& execution_unit__stall_time,
                                    const Time& branch_speculation__violation_stall_time,
                                    const Time& load_speculation__violation_stall_time);

   // Power/Area modeling
   void initializeMcPATInterface(UInt32 num_load_buffer_entries, UInt32 num_store_buffer_entries);
   void updateMcPATCounters(Instruction* ins);

private:
   UInt64 _instruction_count;
   double _average_frequency;
   Time _total_time;
   Time _checkpointed_time;
   UInt64 _total_cycles;

   typedef boost::circular_buffer<DynamicMemoryInfo> DynamicMemoryInfoQueue;
   typedef boost::circular_buffer<DynamicBranchInfo> DynamicBranchInfoQueue;
   typedef boost::circular_buffer<Instruction*> InstructionQueue;

   InstructionQueue _instruction_queue;
   DynamicMemoryInfoQueue _dynamic_memory_info_queue;
   DynamicBranchInfoQueue _dynamic_branch_info_queue;

   bool _enabled;

   // Latency table
   Time _latency_table[16];
   Time _ONE_CYCLE;

   // Memory fence counters
   UInt64 _total_fence_instructions;
   // Pipeline stall counters
   Time _total_instruction_fetch__stall_time;
   Time _total_memory_access__stall_time;
   Time _total_load_queue__stall_time;
   Time _total_store_queue__stall_time;
   Time _total_execution_unit__stall_time;
   // Branch/load speculation
   Time _total_branch_speculation_violation__stall_time;
   Time _total_load_speculation_violation__stall_time;
   // Dynamic instruction stall counters
   Time _total_netrecv__stall_time;
   Time _total_sync__stall_time;
   Time _total__idle_time;

   // Power/Area modeling
   McPATCoreInterface* _mcpat_core_interface;
  
   // Main instruction handling function
   virtual void handleInstruction(Instruction* ins) = 0;
   virtual void handleDynamicInstruction(DynamicInstruction* ins) = 0;
   
   // Instruction latency table
   void initializeLatencyTable(double frequency);
   void updateLatencyTable(double frequency);

   // Memory fence / Dynamic instruction / Pipeline stall counters
   void initializeMemoryFenceCounters();
   void initializeStallCounters();
};
