#pragma once

#include "pin.H"
#include "syscall_model.h"

namespace lite
{

void handleFutexSyscall(CONTEXT* ctx);
void syscallEnterRunModel(THREADID threadIndex, CONTEXT* ctx, SYSCALL_STANDARD syscall_standard, void* v);
void syscallExitRunModel(THREADID threadIndex, CONTEXT* ctx, SYSCALL_STANDARD syscall_standard, void* v);

SyscallMdl::syscall_args_t syscallArgs(CONTEXT *ctxt, SYSCALL_STANDARD syscall_standard);
}
