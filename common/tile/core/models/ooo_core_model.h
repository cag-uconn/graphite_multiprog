#pragma once

#include <iostream>
using std::ostream;
using std::pair;

#include "core_model.h"
/*
  Out-of-order core model.
 */
class OOOCoreModel : public CoreModel
{
private:
   typedef vector<Time> Scoreboard;

public:
   OOOCoreModel(Core* core);
   ~OOOCoreModel();

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

   class RegisterRenameStage
   {
   public:
      RegisterRenameStage(CoreModel* core_model);

      void handle(const Time& decode_ready);
      Time sync(const Time& next_stage_timestamp);
      const Time& getTimeStamp() { return _timestamp; }

   private:
      CoreModel* _core_model;
      Time _timestamp;
   };

   class AllocateStage
   {
   public:
      AllocateStage(CoreModel* core_model);

      void handle(const Time& rename_ready, const Time& allocate_latency);
      Time sync(const Time& next_stage_timestamp);
      const Time& getTimeStamp() { return _timestamp; }
      void outputSummary(ostream& os);

   private:
      CoreModel* _core_model;
      Time _timestamp;
      Time _total_stall_time;
   };

   class ReorderBuffer
   {
   public:
      ReorderBuffer(CoreModel* core_model);
      ~ReorderBuffer();

      Time allocate(const MicroOp& micro_op, const Time& schedule_time);
      void complete(const MicroOp& micro_op, const Time& completion_time);
      void outputSummary(ostream& os);

   private:
      CoreModel* _core_model;
      Scoreboard _scoreboard;
      UInt32 _num_entries;
      UInt32 _allocate_idx;
      Time _total_stall_time;
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
      pair<Time,Time> issueLoad(const Time& issue_time);
      Time issueStore(const Time& issue_time);
      void handleFence(MicroOp::Type micro_op_type);
      void outputSummary(ostream& os);
      
   private:
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

      CoreModel* _core_model;
      LoadQueue* _load_queue;
      StoreQueue* _store_queue;
      Time _total_load_queue__stall_time;
      Time _total_store_queue__stall_time;
   };


private:
   static const UInt32 _NUM_REGISTERS = 512;
   
   InstructionFetchStage* _instruction_fetch_stage;
   InstructionDecodeStage* _instruction_decode_stage;
   RegisterRenameStage* _register_rename_stage;
   AllocateStage* _allocate_stage;
   ReorderBuffer* _reorder_buffer;
   ExecutionUnit* _execution_unit;
   LoadStoreUnit* _load_store_unit;
   Scoreboard _register_scoreboard;

   McPATCoreInterface* _mcpat_core_interface;
   
   void handleInstruction(Instruction *instruction);
};
