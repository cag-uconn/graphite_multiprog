// Jonathan Eastep, Harshad Kasture, Jason Miller, Chris Celio, Charles Gruenwald,
// Nathan Beckmann, David Wentzlaff, James Psota
// 10.12.08
//
// Carbon Computer Simulator
//
// This simulator models future multi-core computers with thousands of cores.
// It runs on today's x86 multicores and will scale as more and more cores
// and better inter-core communications mechanisms become available.
// The simulator provides a platform for research in processor architecture,
// compilers, network interconnect topologies, and some OS.
//
// The simulator runs on top of Intel's Pin dynamic binary instrumention engine.
// Application code in the absence of instrumentation runs more or less
// natively and is thus high performance. When instrumentation is used, models
// can be hot-swapped or dynamically enabled and disabled as desired so that
// performance tracks the level of simulation detail needed.

#include <iostream>
#include <assert.h>
#include <set>
#include <sys/syscall.h>
#include <unistd.h>

#include "pin.H"
#include "log.h"
#include "run_models.h"
#include "analysis.h"
#include "routine_replace.h"

// FIXME: This list could probably be trimmed down a lot.
#include "simulator.h"
#include "core_manager.h"
#include "core.h"
#include "syscall_model.h"
#include "thread_manager.h"
#include "config_file.hpp"
#include "handle_args.h"
#include "thread_start.h"
#include "pin_config.h"
#include "log.h"
#include "vm_manager.h"
#include "performance_modeler.h"

#include "redirect_memory.h"
#include "handle_syscalls.h"
#include "opcodes.h"
#include <typeinfo>

// ---------------------------------------------------------------
// FIXME: 
// There should be a better place to keep these globals
// -- a PinSimulator class or smthg
bool done_app_initialization = false;
config::ConfigFile *cfg;

ADDRINT initial_reg_esp;

// clone stuff
extern int *parent_tidptr;
extern struct user_desc *newtls;
extern int *child_tidptr;
extern PIN_LOCK clone_memory_update_lock;
// ---------------------------------------------------------------

// ---------------------------------------------------------------
// FIXME: 
map <ADDRINT, string> rtn_map;
PIN_LOCK rtn_map_lock;

void printRtn (ADDRINT rtn_addr, bool enter)
{
   GetLock (&rtn_map_lock, 1);
   map<ADDRINT, string>::iterator it = rtn_map.find (rtn_addr);

   string point = enter ? "Enter" : "Exit";
   if (it != rtn_map.end())
   {
      LOG_PRINT ("Routine %s %s", (it->second).c_str(), point.c_str());
   }
   else
   {
      LOG_PRINT ("Routine UNKNOWN %s", point.c_str());
   }
      
   ReleaseLock (&rtn_map_lock);
}
// ---------------------------------------------------------------


INT32 usage()
{
   cerr << "This tool implements a multicore simulator." << endl;
   cerr << KNOB_BASE::StringKnobSummary() << endl;

   return -1;
}

void initializeSyscallModeling ()
{
   // Initialize clone stuff
   parent_tidptr = NULL;
   newtls = NULL;
   child_tidptr = NULL;
   InitLock (&clone_memory_update_lock);
}

void routineCallback(RTN rtn, void *v)
{
   string rtn_name = RTN_Name(rtn);
   
   replaceUserAPIFunction(rtn, rtn_name);

   // ---------------------------------------------------------------
   // FIXME: 
   RTN_Open (rtn);
   
   ADDRINT rtn_addr = RTN_Address (rtn);
   
   GetLock (&rtn_map_lock, 1);
   
   rtn_map.insert (make_pair (rtn_addr, rtn_name));

   ReleaseLock (&rtn_map_lock);
   
   RTN_InsertCall (rtn, IPOINT_BEFORE,
         AFUNPTR (printRtn),
         IARG_ADDRINT, rtn_addr,
         IARG_BOOL, true,
         IARG_END);

   RTN_InsertCall (rtn, IPOINT_AFTER,
         AFUNPTR (printRtn),
         IARG_ADDRINT, rtn_addr,
         IARG_BOOL, false,
         IARG_END);

   RTN_Close (rtn);

   // ---------------------------------------------------------------

   if (rtn_name == "CarbonSpawnThreadSpawner")
   {
      RTN_Open (rtn);

      RTN_InsertCall (rtn, IPOINT_BEFORE,
            AFUNPTR (setupCarbonSpawnThreadSpawnerStack),
            IARG_CONTEXT,
            IARG_END);

      RTN_Close (rtn);
   }

   else if (rtn_name == "CarbonThreadSpawner")
   {
      RTN_Open (rtn);

      RTN_InsertCall (rtn, IPOINT_BEFORE,
            AFUNPTR (setupCarbonThreadSpawnerStack),
            IARG_CONTEXT,
            IARG_END);

      RTN_Close(rtn);
   }
   
   // TODO:
   // Commenting out performance modeling code since it causes multiple accesses to memory
   // when we are simulating shared memory. Fix perf model code to not cause any memory accesses
   //  
   // bool did_func_replace = replaceUserAPIFunction(rtn, rtn_name);
   // if (!did_func_replace)
   //    replaceInstruction(rtn, rtn_name);
}

void handleBasicBlock(BasicBlock *sim_basic_block)
{
    Sim()->getPerformanceModeler()->getPerformanceModel()->queueBasicBlock(sim_basic_block);

    //FIXME: put this in a thread
    Sim()->getPerformanceModeler()->getPerformanceModel()->iterate();
}

void showInstructionInfo(INS ins)
{
   printf("\t");
//   printf("%d - %s ", INS_Category(ins), CATEGORY_StringShort(INS_Category(ins)).c_str());
   printf("%x - %s ", INS_Opcode(ins), OPCODE_StringShort(INS_Opcode(ins)).c_str());
   printf(" %s ", INS_Disassemble(ins).c_str());
   if(INS_IsMemoryWrite(ins))
   printf("\n");
}

VOID fillOperandList(OperandList *list, INS ins)
{
   // for handling register operands
   bool is_mem_read = INS_IsMemoryRead(ins);
   bool is_mem_read2 = INS_HasMemoryRead2(ins);
   unsigned int read_mem_count = 0;

   // for handling memory operands
   unsigned int reg_count = 0;
   unsigned int read_reg_count = 0;
   unsigned int write_reg_count = 0;
   unsigned int max_read_regs = INS_MaxNumRRegs(ins);
//   unsigned int max_write_regs = INS_MaxNumRRegs(ins);

   for(unsigned int i = 0; i < INS_OperandCount(ins); i++)
   {
       if(INS_OperandIsMemory(ins, i))
       {
           OperandDirection dir;
           if(is_mem_read && read_mem_count == 0)
           {
               dir = OPERAND_READ;
               read_mem_count++;
           }
           else if(is_mem_read2 && read_mem_count == 1)
           {
               dir = OPERAND_READ;
               read_mem_count++;
           }
           else
           {
               dir = OPERAND_WRITE;
           }
           list->push_back(Operand(OPERAND_MEMORY, 0, dir));
       }
       else
       {
           if(read_reg_count < max_read_regs)
           {
               list->push_back(Operand(OPERAND_REG, INS_RegR(ins, read_reg_count), OPERAND_READ));
               read_reg_count++;
           }
           else
           {
               list->push_back(Operand(OPERAND_REG, INS_RegW(ins, write_reg_count), OPERAND_WRITE));
               write_reg_count++;
           }
           reg_count++;
       }
   }
}

VOID addInstructionModeling(INS ins)
{
   BasicBlock *basic_block = new BasicBlock();

   // Just use stubs for the operands for now
   Operand a(OPERAND_REG, 0);
   Operand b(OPERAND_REG, 0);
   Operand c(OPERAND_REG, 0);

   if(INS_OperandCount(ins) > 0)
       a = INS_OperandIsMemory(ins, 0) ? Operand(OPERAND_MEMORY, 0) : Operand(OPERAND_REG, INS_OperandReg(ins, 0));
   if(INS_OperandCount(ins) > 1)
       b = INS_OperandIsMemory(ins, 1) ? Operand(OPERAND_MEMORY, 0) : Operand(OPERAND_REG, INS_OperandReg(ins, 1));
   if(INS_OperandCount(ins) > 2)
       c = INS_OperandIsMemory(ins, 2) ? Operand(OPERAND_MEMORY, 0) : Operand(OPERAND_REG, INS_OperandReg(ins, 2));

   // Now handle instructions which have a static cost
   switch(INS_Opcode(ins))
   {
       case OPCODE_DIV:
           basic_block->push_back(new ArithInstruction(INST_DIV, a, b, c));
           break;
       case OPCODE_MUL:
           basic_block->push_back(new ArithInstruction(INST_MUL, a, b, c));
           break;
       case OPCODE_FDIV:
           basic_block->push_back(new ArithInstruction(INST_FDIV, a, b, c));
           break;
       case OPCODE_FMUL:
           basic_block->push_back(new ArithInstruction(INST_FMUL, a, b, c));
           break;
       default:
       {
           OperandList *list = new OperandList();
           fillOperandList(list, ins);
           basic_block->push_back(new GenericInstruction(list));
       }
   }

   INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(handleBasicBlock), IARG_PTR, basic_block, IARG_END);
}


VOID instructionCallback (INS ins, void *v)
{
   INS_InsertCall(ins, IPOINT_BEFORE,
         AFUNPTR(printInsInfo),
         IARG_CALL_ORDER, CALL_ORDER_FIRST,
         IARG_CONTEXT,
         IARG_END);

   addInstructionModeling(ins);

   if (INS_IsSyscall(ins))
   {
      INS_InsertCall(ins, IPOINT_BEFORE,
            AFUNPTR(handleFutexSyscall),
            IARG_CONTEXT,
            IARG_END);
   }

   // Emulate Stack Operations
   bool string_op = rewriteStringOp (ins);

   if (!string_op)
   {
      // Emulate stack operations
      bool stack_op = rewriteStackOp (ins);

      // Else, redirect memory to the simulated memory system
      if (!stack_op)
      {
         rewriteMemOp (ins);
      }
   }
}

// syscall model wrappers

void SyscallEntry(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, void *v)
{
   syscallEnterRunModel (ctxt, std);
}

void SyscallExit(THREADID threadIndex, CONTEXT *ctxt, SYSCALL_STANDARD std, void *v)
{
   syscallExitRunModel (ctxt, std);
}

void ApplicationStart()
{
}

void ApplicationExit(int, void*)
{
   LOG_PRINT("Application exit.");
   Simulator::release();
   delete cfg;
}

VOID threadStartCallback(THREADID threadIndex, CONTEXT *ctxt, INT32 flags, VOID *v)
{
   ADDRINT reg_esp = PIN_GetContextReg(ctxt, REG_STACK_PTR);

   // Conditions under which we must initialize a core
   // 1) (!done_app_initialization) && (curr_process_num == 0)
   // 2) (done_app_initialization) && (!thread_spawner)

   if (! done_app_initialization)
   {
#ifdef REDIRECT_MEMORY
      allocateStackSpace();
#endif

      UInt32 curr_process_num = Sim()->getConfig()->getCurrentProcessNum();
      ADDRINT reg_esp = PIN_GetContextReg(ctxt, REG_STACK_PTR);

      if (curr_process_num == 0)
      {
         Sim()->getCoreManager()->initializeThread(0);

#ifdef REDIRECT_MEMORY
         ADDRINT reg_eip = PIN_GetContextReg(ctxt, REG_INST_PTR);
         // 1) Copying over Static Data
         // Get the image first
         PIN_LockClient();
         IMG img = IMG_FindByAddress(reg_eip);
         PIN_UnlockClient();

         LOG_PRINT("Process: 0, Start Copying Static Data\n");
         copyStaticData(img);
         LOG_PRINT("Process: 0, Finished Copying Static Data\n");

         // 2) Copying over initial stack data
         LOG_PRINT("Process: 0, Start Copying Initial Stack Data\n");
         copyInitialStackData(reg_esp, 0);
         LOG_PRINT("Process: 0, Finished Copying Initial Stack Data\n");
#endif
      }

      else
      {
         core_id_t core_id = Sim()->getConfig()->getCurrentThreadSpawnerCoreNum();
         Sim()->getCoreManager()->initializeThread(core_id);
         
         // FIXME: 
         // Even if this works, it's a hack. We will need this to be a 'ring' where
         // all processes initialize one after the other
         Core *core = Sim()->getCoreManager()->getCurrentCore();
         core->getNetwork()->netRecv (0, SYSTEM_INITIALIZATION_NOTIFY);

         LOG_PRINT("Process: %i, Start Copying Initial Stack Data\n");
         copyInitialStackData(reg_esp, core_id);
         LOG_PRINT("Process: %i, Finished Copying Initial Stack Data\n");
      }
      
      // All the real initialization is done in 
      // replacement_start at the moment
      done_app_initialization = true;

      // Set the current ESP accordingly
      PIN_SetContextReg(ctxt, REG_STACK_PTR, reg_esp);
   }
   else
   {
      // This is NOT the main thread
      // 'application' thread or 'thread spawner'

      core_id_t core_id = PinConfig::getSingleton()->getCoreIDFromStackPtr(reg_esp);

      LOG_ASSERT_ERROR(core_id != -1, "All application threads and thread spawner are cores now");

      if (core_id == Sim()->getConfig()->getCurrentThreadSpawnerCoreNum())
      {
         // 'Thread Spawner' thread
         Sim()->getCoreManager()->initializeThread(core_id);
      }
      else
      {
         // 'Application' thread
         ThreadSpawnRequest* req = Sim()->getThreadManager()->getThreadSpawnReq();

         LOG_ASSERT_ERROR (req != NULL, "ThreadSpawnRequest is NULL !!")

         // This is an application thread
         LOG_ASSERT_ERROR(core_id == req->core_id, "Got 2 different core_ids: req->core_id = %i, core_id = %i", req->core_id, core_id);

         Sim()->getThreadManager()->onThreadStart(req);
      }
     
      // Restore the clone syscall arguments
      PIN_SetContextReg (ctxt, REG_GDX, (ADDRINT) parent_tidptr);
      PIN_SetContextReg (ctxt, REG_GSI, (ADDRINT) newtls);
      PIN_SetContextReg (ctxt, REG_GDI, (ADDRINT) child_tidptr);

      Core *core = Sim()->getCoreManager()->getCurrentCore();
      assert (core);

      // Wait to make sure that the spawner has written stuff back to memory
      // FIXME: What is this for(?) This seems arbitrary
      GetLock (&clone_memory_update_lock, 2);
      ReleaseLock (&clone_memory_update_lock);
   }
}

VOID threadFiniCallback(THREADID threadIndex, const CONTEXT *ctxt, INT32 flags, VOID *v)
{
   Sim()->getThreadManager()->onThreadExit();
}


int main(int argc, char *argv[])
{
   // ---------------------------------------------------------------
   // FIXME: 
   InitLock (&rtn_map_lock);
   // ---------------------------------------------------------------

   // Global initialization
   PIN_InitSymbols();
   PIN_Init(argc,argv);

   string_vec args;

   // Set the default config path if it isn't 
   // overwritten on the command line.
   std::string config_path = "carbon_sim.cfg";

   parse_args(args, config_path, argc, argv);

   cfg = new config::ConfigFile();
   cfg->load(config_path);

   handle_args(args, *cfg);

   Simulator::setConfig(cfg);

   Simulator::allocate();
   Sim()->start();

   PinConfig::allocate();

   VMManager::allocate();

   // Instrumentation
   LOG_PRINT("Start of instrumentation.");
   RTN_AddInstrumentFunction(routineCallback, 0);

   PIN_AddThreadStartFunction (threadStartCallback, 0);
   PIN_AddThreadFiniFunction (threadFiniCallback, 0);
   
   if(cfg->getBool("general/enable_syscall_modeling"))
   {
      initializeSyscallModeling();

      PIN_AddSyscallEntryFunction(SyscallEntry, 0);
      PIN_AddSyscallExitFunction(SyscallExit, 0);
      PIN_AddContextChangeFunction (contextChange, NULL);
   }

   if (cfg->getBool("general/enable_shared_mem"))
   {
      INS_AddInstrumentFunction (instructionCallback, 0);
   }

   PIN_AddFiniFunction(ApplicationExit, 0);

   // Just in case ... might not be strictly necessary
   Transport::getSingleton()->barrier();

   // Never returns
   LOG_PRINT("Running program...");
   PIN_StartProgram();

   return 0;
}
