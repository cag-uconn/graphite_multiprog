#ifndef PTHREAD_THREAD_H
#define PTHREAD_THREAD_H

#include "thread.h"
#include <pthread.h>

class PthreadThread : public Thread
{
public:
   PthreadThread(ThreadFunc func, void *param);
   ~PthreadThread();
   void spawn();
   void join();

private:
   pthread_t _thread;
   ThreadFunc _func;
   void *_param;
};

#endif // PTHREAD_THREAD_H
