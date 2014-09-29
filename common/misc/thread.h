#ifndef THREAD_H
#define THREAD_H

class Runnable
{
public:
   virtual ~Runnable() { }
   static void threadFunc(void *vpRunnable)
   {
      Runnable *runnable = (Runnable*)vpRunnable;
      runnable->run();
   }

private:
   virtual void run() = 0;
};

class Thread
{
public:
   typedef void (*ThreadFunc)(void*);

   static Thread *create(ThreadFunc func, void *param);
   static Thread *create(Runnable *runnable)
   {
      return create(Runnable::threadFunc, runnable);
   }

   virtual ~Thread() { };

   virtual void spawn() = 0;
   virtual void join() = 0;
};

#endif // THREAD_H
