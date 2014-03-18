#include "handle_syscalls.h"
#include "fixed_types.h"
#include "syscall_model.h"
#include "simulator.h"
#include "core.h"
#include "tile_manager.h"
#include <syscall.h>
#include "redirect_memory.h"
#include "vm_manager.h"

// ----------------------------
// Here to handle rt_sigaction syscall
#include <signal.h>

// ----------------------------
// Here to handle nanosleep, gettimeofday syscall
#include <time.h>

// FIXME
// Here to check up on the poll syscall
#include <poll.h>
// Here to check up on the mmap syscall
#include <sys/mman.h>

// ---------------------------------------------------------------
// Here for the uname syscall
#include <sys/utsname.h>

// ---------------------------------------------------------------
// Here for the ugetrlimit, futex, gettimeofday system call
#include <sys/time.h>
#include <sys/resource.h>

// ---------------------------------------------------------------
// Here for the set_thread_area system call
#include <linux/unistd.h>
#include <asm/ldt.h>

// ---------------------------------------------------------------
// Here for the futex system call
#include <linux/futex.h>
#include <sys/time.h>

// ---------------------------------------------------------------
// Here for the fstat64 system call
#include <sys/stat.h>
#include <sys/types.h>

// ---------------------------------------------------------------
// Here for the arch_prctl system call
#include <asm/prctl.h>
#include <sys/prctl.h>

// -----------------------------------
// Clone stuff
#include <sched.h>

// FIXME: 
// These really should be in a class instead of being globals like this
int *parent_tidptr = NULL;
int *child_tidptr = NULL;

PIN_LOCK clone_memory_update_lock;

// End Clone stuff
// -----------------------------------

VOID handleFutexSyscall(CONTEXT *ctx)
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
   
   Core *core = Sim()->getTileManager()->getCurrentCore();
   LOG_ASSERT_ERROR(core != NULL, "Core(NULL)");
   LOG_PRINT("syscall_number %d", syscall_number);

   core->getSyscallMdl()->runEnter(syscall_number, args);
}

void syscallEnterRunModel(THREADID threadIndex, CONTEXT *ctx, SYSCALL_STANDARD syscall_standard, void* v)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   assert(core);
   IntPtr syscall_number = PIN_GetSyscallNumber (ctx, syscall_standard);
   LOG_PRINT("syscall_number %d", syscall_number);

   // Save the syscall number
   core->getSyscallMdl()->saveSyscallNumber(syscall_number);
  
   switch (syscall_number)
   {
   // File operations
   case SYS_open:
   case SYS_read:
   case SYS_write:
   case SYS_writev:
   case SYS_close:
   case SYS_lseek:
   case SYS_access:
   case SYS_rmdir:
   case SYS_unlink:
   case SYS_getcwd:
   case SYS_stat:
   case SYS_fstat:
   case SYS_lstat:
   case SYS_ioctl:
   case SYS_readahead:
   case SYS_pipe:

   // Get process ID
   case SYS_getpid:
   
   // Time manipulation
   case SYS_time:
   case SYS_gettimeofday:
   case SYS_clock_gettime:
   case SYS_clock_getres:

   // Scheduling
   case SYS_sched_setaffinity:
   case SYS_sched_getaffinity:

   // Memory management
   case SYS_brk:
   case SYS_mmap:
   case SYS_munmap:

      {
         SyscallMdl::syscall_args_t args = syscallArgs (ctx, syscall_standard);
         IntPtr new_syscall = core->getSyscallMdl()->runEnter(syscall_number, args);
         PIN_SetSyscallNumber (ctx, syscall_standard, new_syscall);
      }
      break;

   case SYS_futex:
   case SYS_mprotect:
   case SYS_madvise:
   case SYS_set_tid_address:
   case SYS_set_robust_list:
      PIN_SetSyscallNumber (ctx, syscall_standard, SYS_getpid);
      break;

   case SYS_rt_sigprocmask:
      modifyRtsigprocmaskContext (ctx, syscall_standard);
      break;
   
   case SYS_rt_sigsuspend:
      modifyRtsigsuspendContext (ctx, syscall_standard);
      break;
   
   case SYS_rt_sigaction:
      modifyRtsigactionContext (ctx, syscall_standard);
      break;
   
   case SYS_nanosleep:
      modifyNanosleepContext (ctx, syscall_standard);
      break;

   case SYS_uname:
      modifyUnameContext (ctx, syscall_standard);
      break;

   case SYS_set_thread_area:
      modifySet_thread_areaContext (ctx, syscall_standard);
      break;
   
   case SYS_clone:
      modifyCloneContext (ctx, syscall_standard);
      break;

   case SYS_arch_prctl:
      modifyArch_prctlContext (ctx, syscall_standard);
      break;

   case SYS_getrlimit:
      modifyGetrlimitContext (ctx, syscall_standard);
      break;
   
   case SYS_exit:
   case SYS_exit_group:
   case SYS_kill:
   case SYS_gettid:

   case SYS_geteuid:
   case SYS_getuid:
   case SYS_getegid:
   case SYS_getgid:
      // Let the syscall fall through
      break;
   
   default:
      {
         SyscallMdl::syscall_args_t args = syscallArgs (ctx, syscall_standard);
         LOG_PRINT_ERROR ("Unhandled syscall[enter] %d at RIP(%p)\n, "
                          "arg0(%p), arg1(%p), arg2(%p), arg3(%p), arg4(%p), arg5(%p)",
                          syscall_number, PIN_GetContextReg(ctx, REG_INST_PTR),
                          args.arg0, args.arg1, args.arg2, args.arg3, args.arg4, args.arg5);
      }
      break;
   }
}

void syscallExitRunModel(THREADID threadIndex, CONTEXT *ctx, SYSCALL_STANDARD syscall_standard, void* v)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   assert(core);
   IntPtr syscall_number = core->getSyscallMdl()->retrieveSyscallNumber();
  
   switch (syscall_number)
   {
   case SYS_open:
   case SYS_read:
   case SYS_write:
   case SYS_writev:
   case SYS_close:
   case SYS_lseek:
   case SYS_access:
   case SYS_rmdir:
   case SYS_unlink:
   case SYS_getcwd:
   case SYS_stat:
   case SYS_fstat:
   case SYS_lstat:
   case SYS_ioctl:
   case SYS_readahead:
   case SYS_pipe:
   
   // Get process ID
   case SYS_getpid:
   
   // Time manipulation
   case SYS_time:
   case SYS_gettimeofday:
   case SYS_clock_gettime:
   case SYS_clock_getres:
   
   // Scheduling
   case SYS_sched_setaffinity:
   case SYS_sched_getaffinity:
   
   // Memory management
   case SYS_brk:
   case SYS_mmap:
   case SYS_munmap:
  
   // Inter-thread synchronization 
   case SYS_futex:
      
      {
         IntPtr old_return_val = PIN_GetSyscallReturn (ctx, syscall_standard);
         IntPtr syscall_return = core->getSyscallMdl()->runExit(old_return_val);
         PIN_SetContextReg (ctx, REG_GAX, syscall_return);

         LOG_PRINT("Syscall(%p) returned (%p)", syscall_number, syscall_return);
      }
      break;

   case SYS_mprotect:
   case SYS_madvise:
   case SYS_set_robust_list:
      PIN_SetContextReg (ctx, REG_GAX, 0);
      break;
   
   case SYS_set_tid_address:
      break;
   
   case SYS_rt_sigprocmask:
      restoreRtsigprocmaskContext (ctx, syscall_standard);
      break;
   
   case SYS_rt_sigsuspend:
      restoreRtsigsuspendContext (ctx, syscall_standard);
      break;
   
   case SYS_rt_sigaction:
      restoreRtsigactionContext (ctx, syscall_standard);
      break;
   
   case SYS_nanosleep:
      restoreNanosleepContext (ctx, syscall_standard);
      break;

   case SYS_uname:
      restoreUnameContext (ctx, syscall_standard);
      break;
  
   case SYS_set_thread_area:
      restoreSet_thread_areaContext (ctx, syscall_standard);
      break;

   case SYS_clone:
      restoreCloneContext (ctx, syscall_standard);
      break;

   case SYS_arch_prctl:
      restoreArch_prctlContext (ctx, syscall_standard);
      break;

   case SYS_getrlimit:
      restoreGetrlimitContext (ctx, syscall_standard);
      break;
   
   case SYS_gettid:
   case SYS_geteuid:
   case SYS_getuid:
   case SYS_getegid:
   case SYS_getgid:
      // Let the syscall fall through
      break;

   default:
      LOG_PRINT_ERROR("Unhandled syscall[exit] %d", syscall_number);
      break;
   }
}

void contextChange (THREADID threadIndex, CONTEXT_CHANGE_REASON context_change_reason, const CONTEXT *from, CONTEXT *to, INT32 info, VOID *v)
{
   if (context_change_reason == CONTEXT_CHANGE_REASON_SIGNAL)
   {
      ADDRINT esp_to = PIN_GetContextReg (to, REG_STACK_PTR);
      ADDRINT esp_from = PIN_GetContextReg (from, REG_STACK_PTR);
     
      // Copy over things that the kernel wrote on the stack to
      // the simulated stack
      if (esp_to != esp_from)
      {
         Core *core = Sim()->getTileManager()->getCurrentCore();
         if (core)
         {
            core->accessMemory (Core::NONE, Core::WRITE, esp_to, (char*) esp_to, esp_from - esp_to);
         }
      }
   }
   
   else if (context_change_reason == CONTEXT_CHANGE_REASON_SIGRETURN)
   {
   }

   else if (context_change_reason == CONTEXT_CHANGE_REASON_FATALSIGNAL)
   {
      LOG_PRINT_ERROR("Application received fatal signal %u at eip %p\n", info, (void*) PIN_GetContextReg (from, REG_INST_PTR));
   }

   return;
}

void modifyRtsigprocmaskContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args = syscallArgs (ctxt, syscall_standard);
      core->getSyscallMdl()->saveSyscallArgs (args);

      sigset_t *set = (sigset_t*) args.arg1;
      sigset_t *oset = (sigset_t*) args.arg2;

      if (set)
      {
         sigset_t *set_arg = (sigset_t*) core->getSyscallMdl()->copyArgToBuffer (1, (IntPtr) set, sizeof (sigset_t));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 1, (ADDRINT) set_arg);
      }

      if (oset)
      {
         sigset_t *oset_arg = (sigset_t*) core->getSyscallMdl()->copyArgToBuffer (2, (IntPtr) oset, sizeof (sigset_t));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 2, (ADDRINT) oset_arg);
      }
   }
}

void restoreRtsigprocmaskContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args;
      core->getSyscallMdl()->retrieveSyscallArgs (args);
      sigset_t *set = (sigset_t*) args.arg1;
      sigset_t *oset = (sigset_t*) args.arg2;
      if (oset)
      {
         core->getSyscallMdl()->copyArgFromBuffer (2, (IntPtr) oset, sizeof (sigset_t));
      }
      PIN_SetSyscallArgument (ctxt, syscall_standard, 1, (ADDRINT) set);
      PIN_SetSyscallArgument (ctxt, syscall_standard, 2, (ADDRINT) oset);
   }
}

void modifyRtsigsuspendContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args = syscallArgs (ctxt, syscall_standard);
      core->getSyscallMdl()->saveSyscallArgs (args);

      sigset_t *unewset = (sigset_t*) args.arg0;
      if (unewset)
      {
         sigset_t *unewset_arg = (sigset_t*) core->getSyscallMdl()->copyArgToBuffer (0, (IntPtr) unewset, sizeof (sigset_t));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 0, (ADDRINT) unewset_arg);
      }
   }
}

void restoreRtsigsuspendContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args;
      core->getSyscallMdl()->retrieveSyscallArgs (args);

      sigset_t *unewset = (sigset_t*) args.arg0;
      PIN_SetSyscallArgument (ctxt, syscall_standard, 0, (ADDRINT) unewset);
   }
}

void modifyRtsigactionContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args = syscallArgs (ctxt, syscall_standard);
      core->getSyscallMdl()->saveSyscallArgs (args);
      
      struct sigaction *act = (struct sigaction*) args.arg1;
      struct sigaction *oact = (struct sigaction*) args.arg2;

      if (act)
      {
         struct sigaction *act_arg = (struct sigaction*) core->getSyscallMdl()->copyArgToBuffer (1, (IntPtr) act, sizeof (struct sigaction));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 1, (ADDRINT) act_arg);
      }

      if (oact)
      {
         struct sigaction *oact_arg = (struct sigaction*) core->getSyscallMdl()->copyArgToBuffer (2, (IntPtr) oact, sizeof (struct sigaction));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 2, (ADDRINT) oact_arg);
      }
   }
}

void restoreRtsigactionContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args;
      core->getSyscallMdl()->retrieveSyscallArgs (args);

      struct sigaction *act = (struct sigaction*) args.arg1;
      struct sigaction *oact = (struct sigaction*) args.arg2;

      if (oact)
      {
         core->getSyscallMdl()->copyArgFromBuffer (2, (IntPtr) oact, sizeof (struct sigaction));
      }

      PIN_SetSyscallArgument (ctxt, syscall_standard, 1, (ADDRINT) act);
      PIN_SetSyscallArgument (ctxt, syscall_standard, 2, (ADDRINT) oact);
   }
}

void modifyNanosleepContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore ();
   if (core)
   {
      SyscallMdl::syscall_args_t args = syscallArgs (ctxt, syscall_standard);
      core->getSyscallMdl ()->saveSyscallArgs (args);

      struct timespec *req = (struct timespec*) args.arg0;
      struct timespec *rem = (struct timespec*) args.arg1;

      if (req)
      {
         struct timespec *req_arg = (struct timespec*) core->getSyscallMdl ()->copyArgToBuffer (0, (IntPtr) req, sizeof (struct timespec));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 0, (ADDRINT) req_arg);
      }

      if (rem)
      {
         struct timespec *rem_arg = (struct timespec*) core->getSyscallMdl ()->copyArgToBuffer (1, (IntPtr) rem, sizeof (struct timespec));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 1, (ADDRINT) rem_arg);
      }
   }
}

void restoreNanosleepContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args;
      core->getSyscallMdl()->retrieveSyscallArgs (args);

      struct timespec *req = (struct timespec*) args.arg0;
      struct timespec *rem = (struct timespec*) args.arg1;

      if (rem)
      {
         core->getSyscallMdl()->copyArgFromBuffer (1, (IntPtr) rem, sizeof (struct timespec));
      }

      PIN_SetSyscallArgument (ctxt, syscall_standard, 0, (ADDRINT) req);
      PIN_SetSyscallArgument (ctxt, syscall_standard, 1, (ADDRINT) rem);
   }
}

void modifyUnameContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args = syscallArgs (ctxt, syscall_standard);
      core->getSyscallMdl()->saveSyscallArgs (args);

      struct utsname *buf = (struct utsname*) args.arg0;

      if (buf)
      {
         struct utsname *buf_arg = (struct utsname*) core->getSyscallMdl()->copyArgToBuffer (0, (IntPtr) buf, sizeof (struct utsname));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 0, (ADDRINT) buf_arg);
      }
   }
}

void restoreUnameContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args;
      core->getSyscallMdl()->retrieveSyscallArgs (args);

      struct utsname *buf = (struct utsname*) args.arg0;

      if (buf)
      {
         core->getSyscallMdl()->copyArgFromBuffer(0, (IntPtr) buf, sizeof (struct utsname));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 0, (ADDRINT) buf);
      }
   }
}

void modifySet_thread_areaContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args = syscallArgs (ctxt, syscall_standard);
      core->getSyscallMdl()->saveSyscallArgs (args);

      struct user_desc *uinfo = (struct user_desc*) args.arg0;

      if (uinfo)
      {
         struct user_desc *uinfo_arg = (struct user_desc*) core->getSyscallMdl()->copyArgToBuffer (0, (IntPtr) uinfo, sizeof (struct user_desc));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 0, (ADDRINT) uinfo_arg);
      }
   }
}

void restoreSet_thread_areaContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args;
      core->getSyscallMdl()->retrieveSyscallArgs (args);

      struct user_desc *uinfo = (struct user_desc*) args.arg0;

      if (uinfo)
      {
         core->getSyscallMdl()->copyArgFromBuffer (0, (IntPtr) uinfo, sizeof (struct user_desc));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 0, (ADDRINT) uinfo);
      }
   }
}

void modifyCloneContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args = syscallArgs (ctxt, syscall_standard);
      core->getSyscallMdl()->saveSyscallArgs (args);

      LOG_PRINT("Clone Syscall: flags(0x%x), stack(0x%x), parent_tidptr(0x%x), child_tidptr(0x%x), tls(0x%x)",
            (IntPtr) args.arg0, (IntPtr) args.arg1, (IntPtr) args.arg2, (IntPtr) args.arg3, (IntPtr) args.arg4);

      parent_tidptr = (int*) args.arg2;
      child_tidptr = (int*) args.arg3;

      // Get the lock so that the parent can update simulated memory
      // with values returned by the clone syscall before the child 
      // uses them
      PIN_GetLock (&clone_memory_update_lock, 1);

      if (parent_tidptr)
      {
         int *parent_tidptr_arg = (int*) core->getSyscallMdl()->copyArgToBuffer (2, (IntPtr) parent_tidptr, sizeof (int));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 2, (ADDRINT) parent_tidptr_arg);
      }

      if (child_tidptr)
      {
         int *child_tidptr_arg = (int*) core->getSyscallMdl()->copyArgToBuffer (3, (IntPtr) child_tidptr, sizeof (int));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 3, (ADDRINT) child_tidptr_arg);
      }
   }
}

void restoreCloneContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args;
      core->getSyscallMdl()->retrieveSyscallArgs (args);

      if (parent_tidptr)
      {
         core->getSyscallMdl()->copyArgFromBuffer (2, (IntPtr) parent_tidptr, sizeof(int));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 2, (ADDRINT) parent_tidptr);
      }

      if (child_tidptr)
      {
         core->getSyscallMdl()->copyArgFromBuffer (3, (IntPtr) child_tidptr, sizeof(int));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 3, (ADDRINT) child_tidptr);
      }

      // Release the lock now that we have copied all results to simulated memory
      PIN_ReleaseLock (&clone_memory_update_lock);
   }
}

void modifyGetrlimitContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args = syscallArgs (ctxt, syscall_standard);
      core->getSyscallMdl()->saveSyscallArgs (args);

      struct rlimit *rlim = (struct rlimit*) args.arg1;

      if (rlim)
      {
         struct rlimit *rlim_arg = (struct rlimit*) core->getSyscallMdl()->copyArgToBuffer (1, (IntPtr) rlim, sizeof (struct rlimit));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 1, (ADDRINT) rlim_arg);
      }
   }
}

void restoreGetrlimitContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args;
      core->getSyscallMdl()->retrieveSyscallArgs (args);

      struct rlimit *rlim = (struct rlimit*) args.arg1;

      if (rlim)
      {
         core->getSyscallMdl()->copyArgFromBuffer (1, (IntPtr) rlim, sizeof(struct rlimit));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 1, (ADDRINT) rlim);
      }
   }
}

void modifyArch_prctlContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args = syscallArgs (ctxt, syscall_standard);
      core->getSyscallMdl()->saveSyscallArgs (args);
      
      int code = (int) args.arg0;
      if ((code == ARCH_GET_FS) || (code == ARCH_GET_GS))
      {
         unsigned long *addr = (unsigned long*) args.arg1;
         unsigned long *addr_arg = (unsigned long*) core->getSyscallMdl()->copyArgToBuffer (1, (IntPtr) addr, sizeof (unsigned long));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 1, (ADDRINT) addr_arg);
      }
   }
}

void restoreArch_prctlContext (CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard)
{
   Core *core = Sim()->getTileManager()->getCurrentCore();
   if (core)
   {
      SyscallMdl::syscall_args_t args;
      core->getSyscallMdl()->retrieveSyscallArgs (args);

      int code = (int) args.arg0;
      if ((code == ARCH_GET_FS) || (code == ARCH_GET_GS))
      {
         unsigned long *addr = (unsigned long*) args.arg1;
         core->getSyscallMdl()->copyArgFromBuffer (1, (IntPtr) addr, sizeof (unsigned long));
         PIN_SetSyscallArgument (ctxt, syscall_standard, 1, (ADDRINT) addr);
      }
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

