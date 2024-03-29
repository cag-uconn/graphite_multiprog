#pragma once

#include <iostream>
using std::ostream;
using std::pair;

#include "core_model.h"
#include "load_speculation_handler.h"

// In-order core, out-of-order memory model.
//   We use a simple scoreboard to keep track of registers.
//   We also keep a store buffer to enable load bypassing.

class InOrderCoreModel : public CoreModel
{
private:
   typedef vector<Time> Scoreboard;

public:
   InOrderCoreModel(Core* core);
   ~InOrderCoreModel();

   void outputSummary(ostream &os, const Time& target_completion_time);

   class InstructionFetchStage
   {
   public:
      InstructionFetchStage(CoreModel* core_model);

      void handle(const Instruction* instruction, const Time& start_time);
      const Time& sync(const Time& next_stage_timestamp);

      const Time& getTimeStamp() const { return _timestamp; }
      const Time& getStallTime() const { return _total_stall_time; }
      void outputSummary(ostream& os);
   
   private:
      CoreModel* _core_model;
      Time _timestamp;
      Time _total_stall_time;
   };

   class ExecutionUnit
   {
   public:
      ExecutionUnit(CoreModel* core_model);

      Time handle(const Time& issue_time, Time& commit_time, uint16_t lat);
   private:
      CoreModel* _core_model;
   };

   class BranchUnit
   {
   public:
      BranchUnit(CoreModel* core_model);

      Time handle(const Time& issue_time, Time& commit_time, uint16_t lat,
                  uintptr_t address, BranchPredictor* branch_predictor,
                  bool& speculation_failed);
   private:
      CoreModel* _core_model;
   };

   class StoreQueue
   {
   public:
      enum Scheme
      {
         TWO_PHASES,
         ONE_PHASE,
         SERIALIZE
      };

      StoreQueue(CoreModel* core_model);
      ~StoreQueue();

      void handle(Time& dispatch_time, Time& commit_time);
      
      const Time& getStallTime() const    { return _total_stall_time; }
      void handleFence(Time& dispatch_time, Time& commit_time);
      bool isAddressAvailable(const Time& issue_time, IntPtr address) const;

   private:
      CoreModel* _core_model;
      // Store queue structures
      Scoreboard _scoreboard;
      Scoreboard _operands_scoreboard;
      vector<IntPtr> _addresses;
      UInt32 _num_entries;
      UInt32 _allocate_idx;
      // Multiple outstanding stores?
      bool _multiple_outstanding_stores_enabled;
      // Stall time
      Time _total_stall_time;
      
      Scheme getScheme() const;
      const Time& getLastDeallocateTime();
   };

   class LoadQueue
   {
   public:
      enum Scheme
      {
         IN_PARALLEL,
         SERIALIZE
      };

      LoadQueue(CoreModel* core_model);
      ~LoadQueue();

      Time handle(Time& dispatch_time, Time& commit_time,
                  bool& speculation_failed, const StoreQueue* store_queue);

      const Time& getStallTime() const    { return _total_stall_time; }

      void outputSummaryLoadSpeculation(ostream& os);

   private:
      CoreModel* _core_model;
      // Load queue structure
      Scoreboard _scoreboard;
      UInt32 _num_entries;
      UInt32 _allocate_idx;
      // Multiple outstanding loads?
      bool _multiple_outstanding_loads_enabled;
      // Speculation support?
      bool _speculation_support_enabled;
      // How is load speculation handled?
      LoadSpeculationHandler* _load_speculation_handler;
      // Ordering point for loads
      Time _ordering_point;
      // Stall time
      Time _total_stall_time;

      Scheme getScheme() const;
   };

private:
   InstructionFetchStage* _instruction_fetch_stage;
   ExecutionUnit* _execution_unit;
   BranchUnit* _branch_unit;
   LoadQueue* _load_queue;
   StoreQueue* _store_queue;
   
   // Register scoreboard
   Scoreboard _register_scoreboard;
   vector<MicroOp::Type> _register_dependency_list;

   // Dispatch/Commit times
   Time _dispatch_time;
   Time _commit_time;

   // Memory Consistency Model/Violations
   Time _total_branch_speculation_violation__stall_time;
   Time _total_load_speculation_violation__stall_time;
   // Stall counters
   Time _total_memory_access__stall_time;
   Time _total_execution_unit__stall_time;

   McPATCoreInterface* _mcpat_core_interface;
   
   void handleInstruction(Instruction *instruction);
   void handleDynamicInstruction(DynamicInstruction *instruction);

   // Utilities
   Time getOperandsReady(const MicroOp& micro_op, Time* operands_ready_list) const;
   void updateStallCounters(const MicroOp& micro_op, const Time& stall_time,
                            const Time* operands_ready_list);
   void updateRegisterScoreboard(const MicroOp& micro_op, const Time& results_ready);
};
