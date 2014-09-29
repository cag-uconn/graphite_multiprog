#ifndef PIN_THREAD_H
#define PIN_THREAD_H

#include "thread.h"
#include "pin.H"

class PinThread : public Thread
{
public:
   PinThread(ThreadFunc func, void *param);
   ~PinThread();
   void spawn();
   void join();

private:
   static const int STACK_SIZE=1048576;

   THREADID _thread_id;
   PIN_THREAD_UID _thread_uid;
   ThreadFunc _func;
   void *_param;
};

#endif // PIN_THREAD_H
