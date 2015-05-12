#include "pthread_thread.h"
#include "carbon_user.h"
#include "thread_support_private.h"
#include "log.h"
#include <cassert>

PthreadThread::PthreadThread(ThreadFunc func, void *param)
   : _func(func)
   , _param(param)
{
}

PthreadThread::~PthreadThread()
{
}

void PthreadThread::spawn()
{
   LOG_PRINT("Creating thread at func: %p, arg: %p", _func, _param);
   pthread_attr_t attr;
   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
   
   typedef void* (*PthreadFunc)(void*);
   PthreadFunc func = reinterpret_cast<PthreadFunc>(_func);
   __attribute__((unused)) int ret = pthread_create(&_thread, &attr, func, _param);
   assert(ret == 0);
}

void PthreadThread::join()
{
   LOG_PRINT("Joining on thread: %d", _thread);
   __attribute__((unused)) int ret = pthread_join(_thread, NULL);
   assert(ret == 0);
}

// Check if pin_thread.cc is included in the build and has
// Thread::Create defined. If so, PthreadThread is not used.
__attribute__((weak)) Thread* Thread::create(ThreadFunc func, void *param)
{
   return new PthreadThread(func, param);
}
