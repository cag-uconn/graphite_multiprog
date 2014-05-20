#ifndef MCP_H
#define MCP_H

#include "message_types.h"
#include "packetize.h"
#include "network.h"
#include "vm_manager.h"
#include "syscall_server.h"
#include "sync_server.h"
#include "clock_skew_management_object.h"
#include "fixed_types.h"
#include "thread.h"

class MCP : public Runnable
{
public:
   MCP(Network& network);
   ~MCP();

   void spawnThread();
   void quitThread();

   VMManager* getVMManager()
   { return &_vm_manager; }
   ClockSkewManagementServer* getClockSkewManagementServer()
   { return _clock_skew_management_server; }

private:
   void run();
   void processPacket();
  
   bool _finished; 
   Network& _network;
   UnstructuredBuffer _send_buff;
   UnstructuredBuffer _recv_buff;
   const UInt32 _MCP_SERVER_MAX_BUFF;
   char *_scratch;

   VMManager _vm_manager;
   SyscallServer _syscall_server;
   SyncServer _sync_server;
   ClockSkewManagementServer* _clock_skew_management_server;
  
   Thread* _thread;
};

#endif
