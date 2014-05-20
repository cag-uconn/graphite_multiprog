#include "pin_thread.h"
#include <cassert>

PinThread::PinThread(ThreadFunc func, void *param)
   : _func(func)
   , _param(param)
 {
 }

PinThread::~PinThread()
{
}

void PinThread::spawn()
{
   _thread_id = PIN_SpawnInternalThread(_func, _param, STACK_SIZE, &_thread_uid);
   assert(_thread_id != INVALID_THREADID);
}

void PinThread::join()
{
   __attribute__((unused)) bool ret = PIN_WaitForThreadTermination(_thread_uid, PIN_INFINITE_TIMEOUT, NULL);
   assert(ret);
}

Thread* Thread::create(ThreadFunc func, void *param)
{
   return new PinThread(func, param);
}
