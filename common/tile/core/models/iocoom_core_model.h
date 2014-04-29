#ifndef IOCOOM_CORE_MODEL_H
#define IOCOOM_CORE_MODEL_H

#include "core_model.h"
/*
  In-order core, out-of-order memory model.
  We use a simple scoreboard to keep track of registers.
  We also keep a store buffer to enable load bypassing.
 */
class IOCOOMCoreModel : public CoreModel
{
private:
   enum CoreUnit
   {
      INVALID_UNIT = 0,
      LOAD_UNIT = 1,
      STORE_UNIT = 2,
      EXECUTION_UNIT = 3
   };

   typedef vector<Time> Scoreboard;

public:
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

      void handle(const Time& decode_ready, const Time& register_operands_ready);
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

      void handle(const Time& register_fetch_ready, const Time& register_operands_ready);
      Time sync(const Time& next_stage_timestamp);
      const Time& getTimeStamp() const { return _timestamp; }

   private:
      CoreModel* _core_model;
      Time _timestamp;
   };

   class LoadStoreUnit
   {
   public:
      LoadStoreUnit(CoreModel* core_model);
      ~LoadStoreUnit();

      Time allocateLoad(const Time& schedule_time);
      Time allocateStore(const Time& schedule_time);
      pair<Time,Time> issueLoad(const Time& issue_time);
      Time issueStore(const Time& issue_time);
      void handleFence(MicroOp::Type micro_op_type);
      void outputSummary(ostream& os);
      
      class LoadQueue
      {
      public:
         LoadQueue(CoreModel* core_model);
         ~LoadQueue();

         Time allocate(const Time& schedule_time);
         pair<Time,Time> issue(const Time& issue_time, bool found_in_store_queue, const DynamicMemoryInfo& info);
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

private:

   static const UInt32 _NUM_REGISTERS = 512;
   
   StoreQueue *_store_queue;
   LoadQueue *_load_queue;

   Scoreboard _register_scoreboard;
   vector<CoreUnit> _register_dependency_list;

   Time _ONE_CYCLE;

   McPATCoreInterface* _mcpat_core_interface;
   
   // Pipeline Stall Counters
   Time _total_load_queue_stall_time;
   Time _total_store_queue_stall_time;
   Time _total_l1icache_stall_time;
   Time _total_intra_ins_l1dcache_stall_time;
   Time _total_inter_ins_l1dcache_stall_time;
   Time _total_intra_ins_execution_unit_stall_time;
   Time _total_inter_ins_execution_unit_stall_time;
   
   void handleInstruction(Instruction *instruction);

   pair<Time,Time> executeLoad(const Time& schedule_time, const DynamicMemoryInfo& info);
   Time executeStore(const Time& schedule_time, const DynamicMemoryInfo& info);

   void initializePipelineStallCounters();
};

#endif // IOCOOM_CORE_MODEL_H
