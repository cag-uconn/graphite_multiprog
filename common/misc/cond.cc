#include "lock.h"
#include "cond.h"

#include <unistd.h>
#include <syscall.h>
#include <linux/futex.h>
#include <limits.h>

ConditionVariable::ConditionVariable()
      : _numWaiting(0)
      , _futx(0)
{
}

ConditionVariable::~ConditionVariable()
{
}

void ConditionVariable::acquire()
{
   _lock.acquire();
}

void ConditionVariable::release()
{
   _lock.release();
}

void ConditionVariable::wait()
{
   _numWaiting ++;
   _futx = 0;
   _lock.release();

   syscall(SYS_futex, (void*) &_futx, FUTEX_WAIT, 0, NULL, NULL, 0);

   _lock.acquire();
   _numWaiting --;
}

void ConditionVariable::signal()
{
   _lock.acquire();

   if (_numWaiting > 0)
   {
      _futx = 1;
      syscall(SYS_futex, (void*) &_futx, FUTEX_WAKE, 1, NULL, NULL, 0);
   }

   _lock.release();
}

void ConditionVariable::broadcast()
{
   _lock.acquire();

   if (_numWaiting > 0)
   {
      _futx = 1;
      syscall(SYS_futex, (void*) &_futx, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
   }

   _lock.release();
}
