#include <syscall.h>
using namespace std;

#include "lite/handle_syscalls.h"
#include "simulator.h"
#include "tile_manager.h"
#include "tile.h"
#include "syscall_model.h"
#include "log.h"

namespace lite
{

static bool _application_running = true;

void handleFutexSyscall(CONTEXT* ctx)
{
   ADDRINT syscall_number = PIN_GetContextReg (ctx, REG_GAX);
   if (syscall_number != SYS_futex)
      return;

   SyscallMdl::syscall_args_t args;

   // FIXME: The LEVEL_BASE:: ugliness is required by the fact that REG_R8 etc 
   // are also defined in /usr/include/sys/ucontext.h
   args.arg0 = PIN_GetContextReg (ctx, LEVEL_BASE::REG_GDI);
   args.arg1 = PIN_GetContextReg (ctx, LEVEL_BASE::REG_GSI);
   args.arg2 = PIN_GetContextReg (ctx, LEVEL_BASE::REG_GDX);
   args.arg3 = PIN_GetContextReg (ctx, LEVEL_BASE::REG_R10); 
   args.arg4 = PIN_GetContextReg (ctx, LEVEL_BASE::REG_R8);
   args.arg5 = PIN_GetContextReg (ctx, LEVEL_BASE::REG_R9);

   Core* core = Sim()->getTileManager()->getCurrentCore();
   assert(core);
 
   core->getSyscallMdl()->runEnter(syscall_number, args);
}

void syscallEnterRunModel(THREADID threadIndex, CONTEXT* ctx, SYSCALL_STANDARD syscall_standard, void* v)
{
   Core* core = Sim()->getTileManager()->getCurrentCore();
   assert(core);
   IntPtr syscall_number = PIN_GetSyscallNumber(ctx, syscall_standard);
  
   // Save the syscall number
   core->getSyscallMdl()->saveSyscallNumber(syscall_number);
   
   switch (syscall_number)
   {
   // Process exiting
   case SYS_exit_group:
      {
         _application_running = false; 
         SyscallMdl::syscall_args_t args = syscallArgs(ctx, syscall_standard);
         core->getSyscallMdl()->runEnter(syscall_number, args);
      }
      break;

   // Inter-thread synchronization 
   case SYS_futex:
      {
         if (_application_running)
            PIN_SetSyscallNumber(ctx, syscall_standard, SYS_getpid);
      }
      break;

   // Time manipulation
   case SYS_time:
   case SYS_gettimeofday:
   case SYS_clock_gettime:
   case SYS_clock_getres:
      {
         SyscallMdl::syscall_args_t args = syscallArgs(ctx, syscall_standard);
         IntPtr new_syscall = core->getSyscallMdl()->runEnter(syscall_number, args);
         PIN_SetSyscallNumber (ctx, syscall_standard, new_syscall);
      }
      break;

   default:
      break;
   }
}

void syscallExitRunModel(THREADID threadIndex, CONTEXT* ctx, SYSCALL_STANDARD syscall_standard, void* v)
{
   Core* core = Sim()->getTileManager()->getCurrentCore();
   assert(core);
   IntPtr syscall_number = core->getSyscallMdl()->retrieveSyscallNumber();

   switch (syscall_number)
   {
   // Time manipulation
   case SYS_time:
   case SYS_gettimeofday:
   case SYS_clock_gettime:
   case SYS_clock_getres:
  
   // Inter-thread synchronization 
   case SYS_futex:
   
      {
         IntPtr old_return_val = PIN_GetSyscallReturn (ctx, syscall_standard);
         IntPtr syscall_return = core->getSyscallMdl()->runExit(old_return_val);
         PIN_SetContextReg (ctx, REG_GAX, syscall_return);

         LOG_PRINT("Syscall(%p) returned (%p)", syscall_number, syscall_return);
      }
      break;

   default:
      break;
   }
}

SyscallMdl::syscall_args_t syscallArgs(CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   SyscallMdl::syscall_args_t args;
   args.arg0 = PIN_GetSyscallArgument (ctxt, syscall_standard, 0);
   args.arg1 = PIN_GetSyscallArgument (ctxt, syscall_standard, 1);
   args.arg2 = PIN_GetSyscallArgument (ctxt, syscall_standard, 2);
   args.arg3 = PIN_GetSyscallArgument (ctxt, syscall_standard, 3);
   args.arg4 = PIN_GetSyscallArgument (ctxt, syscall_standard, 4);
   args.arg5 = PIN_GetSyscallArgument (ctxt, syscall_standard, 5);

   return args;
}

}
