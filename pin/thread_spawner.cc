#include "thread_spawner.h"
#include "simulator.h"
#include "tile_manager.h"
#include "core.h"
#include "fixed_types.h"
#include "log.h"

int spawnThreadSpawner(const CONTEXT *ctxt)
{
   int res;

   IntPtr reg_eip = PIN_GetContextReg(ctxt, REG_INST_PTR);

   PIN_LockClient();
   
   AFUNPTR thread_spawner;
   IMG img = IMG_FindByAddress(reg_eip);
   RTN rtn = RTN_FindByName(img, "CarbonSpawnThreadSpawner");
   thread_spawner = RTN_Funptr(rtn);

   PIN_UnlockClient();
   LOG_ASSERT_ERROR(thread_spawner, "ThreadSpawner function is null. You may not have linked to the carbon APIs correctly.");
   
   LOG_PRINT("Starting CarbonSpawnThreadSpawner(%p)", thread_spawner);
   
   PIN_CallApplicationFunction(ctxt,
            PIN_ThreadId(),
            CALLINGSTD_DEFAULT,
            thread_spawner,
            PIN_PARG(int), &res,
            PIN_PARG_END());

   LOG_PRINT("Thread spawner spawned");
   LOG_ASSERT_ERROR(res == 0, "Failed to spawn Thread Spawner");

   return res;
}

int callThreadSpawner(const CONTEXT* ctxt)
{
   int res;
   ADDRINT reg_eip = PIN_GetContextReg (ctxt, REG_INST_PTR);

   PIN_LockClient();

   AFUNPTR thread_spawner;
   IMG img = IMG_FindByAddress(reg_eip);
   RTN rtn = RTN_FindByName(img, "CarbonThreadSpawner");
   thread_spawner = RTN_Funptr(rtn);

   PIN_UnlockClient();
   
   PIN_CallApplicationFunction (ctxt,
         PIN_ThreadId(),
         CALLINGSTD_DEFAULT,
         thread_spawner,
         PIN_PARG(int), &res,
         PIN_PARG(void*), NULL,
         PIN_PARG_END());

   return res;
}

void setupCarbonSpawnThreadSpawnerStack(const CONTEXT *ctxt)
{
   // This will clearly need to change somewhat in the multi-process case
   // We can go back to our original scheme of having the "main" thread 
   // on processes other than 0 execute the thread spawner, in which case
   // this will probably just work as is

   LOG_ASSERT_ERROR(Sim()->getConfig()->isMasterProcess(), "setupCarbonSpawnThreadSpawnerStack() should only be called at master process!  Process-Num: %u",
                    Sim()->getConfig()->getCurrentProcessNum());
   
   ADDRINT esp = PIN_GetContextReg (ctxt, REG_STACK_PTR);
   ADDRINT ret_ip = * (ADDRINT*) esp;

   Core *core = Sim()->getTileManager()->getCurrentCore();
   assert (core);

   core->accessMemory(Core::NONE, Core::WRITE, (IntPtr) esp, (char*) &ret_ip, sizeof (ADDRINT));
}

void setupCarbonThreadSpawnerStack(const CONTEXT *ctxt)
{
   if (Sim()->getConfig()->getCurrentProcessNum() == 0)
      return;

   ADDRINT esp = PIN_GetContextReg (ctxt, REG_STACK_PTR);
   ADDRINT ret_ip = * (ADDRINT*) esp;
   ADDRINT p = * (ADDRINT*) (esp + sizeof (ADDRINT));

   Core *core = Sim()->getTileManager()->getCurrentCore();
   assert (core);

   core->accessMemory (Core::NONE, Core::WRITE, (IntPtr) esp, (char*) &ret_ip, sizeof (ADDRINT));
   core->accessMemory (Core::NONE, Core::WRITE, (IntPtr) (esp + sizeof (ADDRINT)), (char*) &p, sizeof (ADDRINT));
}
