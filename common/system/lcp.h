#ifndef LCP_H
#define LCP_H

#include "thread.h"
#include "network.h"

class LCP : public Runnable
{
public:
   LCP();
   ~LCP();

   void spawnThread();
   void quitThread();

private:
   void run();
   void processPacket();

   void updateCommId(void *vp);

   bool _finished;
   SInt32 _proc_num;
   Transport::Node* _transport;

   Thread* _thread;
};

#endif // LCP_H
