#pragma once

#include "core_model.h"
/*
  In-order core, out-of-order memory model.
  We use a simple scoreboard to keep track of registers.
  We also keep a store buffer to enable load bypassing.
 */
class IOCOOMCoreModel : public CoreModel
{
public:
   enum CoreUnit
   {
      INVALID_UNIT = 0,
      LOAD_UNIT = 1,
      STORE_UNIT = 2,
      EXECUTION_UNIT = 3
   };

   typedef vector<Time> Scoreboard;
   typedef vector<CoreUnit> DependencyTracker;

   IOCOOMCoreModel(Core* core);
   ~IOCOOMCoreModel();

   void outputSummary(ostream &os, const Time& target_completion_time);
   
   class InstructionFetchStage
   {
   public:
      InstructionFetchStage(CoreModel* core_model);

      void handle(const Instruction* instruction, const Time& curr_time);
      Time sync(const Time& next_stage_timestamp);
      const Time& getTimeStamp() const { return _timestamp; }
      const Time& getStallTime() const { return _total_stall_time; }
      void outputSummary(ostream& os);
   
   private:
      CoreModel* _core_model;
      Time _timestamp;
      Time _total_stall_time;
   };

   class InstructionDecodeStage
   {
   public:
      InstructionDecodeStage(CoreModel* core_model);

      void handle(const Time& fetch_ready);
      Time sync(const Time& next_stage_timestamp);
      const Time& getTimeStamp() const { return _timestamp; }

   private:
      CoreModel* _core_model;
      Time _timestamp;
   };

   class RegisterFetchStage
   {
   public:
      RegisterFetchStage(CoreModel* core_model);

      void handle(const Time& decode_ready);
      Time sync(const Time& next_stage_timestamp);
      const Time& getTimeStamp() const { return _timestamp; }

   private:
      CoreModel* _core_model;
      Time _timestamp;
   };

   class DispatchStage
   {
   public:
      DispatchStage(CoreModel* core_model);
      void outputSummary(ostream& os);

      Time getMemoryAccessStallTime() const;
      Time getExecutionUnitStallTime() const;

      void handle(const Instruction* instruction, const Time& register_fetch_ready);
      Time sync(const Time& next_stage_timestamp, const CoreUnit& unit);
      Time update(const Time& prev_micro_op_completion_time, const CoreUnit& wait_unit);
      void updateScoreboard(Instruction* instruction, const Time& completion_time, const CoreUnit& wait_unit);
      
      const Time& getTimeStamp() const { return _timestamp; }

   private:
      CoreModel* _core_model;
      Time _timestamp;

      Scoreboard _register_scoreboard;
      DependencyTracker _register_dependency_list;

      // Detailed Pipeline Stall Counters
      Time _total_load_queue__stall_time;
      Time _total_store_queue__stall_time;
      Time _total_intra_ins__memory_access__stall_time;
      Time _total_inter_ins__memory_access__stall_time;
      Time _total_intra_ins__execution_unit__stall_time;
      Time _total_inter_ins__execution_unit__stall_time;

      void initializePipelineStallCounters();
   };

   class ExecutionUnit
   {
   public:
      Time issue(const MicroOp& micro_op, const Time& issue_time, const Time& cost);
   };

   class LoadStoreUnit
   {
   public:
      LoadStoreUnit(CoreModel* core_model);
      ~LoadStoreUnit();

      Time allocateLoad(const Time& schedule_time);
      Time allocateStore(const Time& schedule_time);
      Time issueLoad(const Time& issue_time);
      void issueStore(const Time& issue_time);
      
      void handleFence(MicroOp::Type micro_op_type);

      const Time& getLastLoadAllocateTime() const
      { return _load_queue->getLastAllocateTime(); }
      const Time& getLastStoreAllocateTime() const
      { return _store_queue->getLastAllocateTime(); }
      
      void outputSummary(ostream& os);

   private:      
      class LoadQueue
      {
      public:
         LoadQueue(CoreModel* core_model);
         ~LoadQueue();

         Time allocate(const Time& schedule_time);
         pair<Time,Time> issue(const Time& issue_time, bool found_in_store_queue, const DynamicMemoryInfo& info);
         const Time& getLastAllocateTime();
         const Time& getLastDeallocateTime();
         void setFenceTime(const Time& fence_time);

      private:
         CoreModel* _core_model;
         Scoreboard _scoreboard;
         UInt32 _num_entries;
         bool _speculative_loads_enabled;
         UInt32 _allocate_idx;
         Time _fence_time;
      };

      class StoreQueue
      {
      public:
         enum Status
         {
            VALID,
            COMPLETED,
            NOT_FOUND
         };

         StoreQueue(CoreModel* core_model);
         ~StoreQueue();

         Time allocate(const Time& schedule_time);
         Time issue(const Time& issue_time, const Time& last_load_deallocate_time, const DynamicMemoryInfo& info);
         const Time& getLastAllocateTime();
         const Time& getLastDeallocateTime();
         void setFenceTime(const Time& fence_time);
         Status isAddressAvailable(const Time& schedule_time, IntPtr address);

      private:
         CoreModel* _core_model;
         Scoreboard _scoreboard;
         vector<IntPtr> _addresses;
         UInt32 _num_entries;
         bool _multiple_outstanding_RFOs_enabled;
         UInt32 _allocate_idx;
         Time _fence_time;
      };
      
      CoreModel* _core_model;
      LoadQueue* _load_queue;
      StoreQueue* _store_queue;
   };

private:

   static const UInt32 _NUM_REGISTERS = 512;
   
   InstructionFetchStage* _instruction_fetch_stage;
   InstructionDecodeStage* _instruction_decode_stage;
   RegisterFetchStage* _register_fetch_stage;
   DispatchStage* _dispatch_stage;
   ExecutionUnit* _execution_unit;
   LoadStoreUnit* _load_store_unit;

   Time _ONE_CYCLE;

   McPATCoreInterface* _mcpat_core_interface;
   
   void handleInstruction(Instruction *instruction);

   pair<Time,Time> executeLoad(const Time& schedule_time, const DynamicMemoryInfo& info);
   Time executeStore(const Time& schedule_time, const DynamicMemoryInfo& info);
};
