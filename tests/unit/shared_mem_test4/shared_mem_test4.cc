#include "tile.h"
#include "core.h"
#include "mem_component.h"
#include "tile_manager.h"
#include "simulator.h"

#include "carbon_user.h"
#include "fixed_types.h"

using namespace std;

void* thread_func(void*);

int num_threads = 64;
int num_iterations = 50;
int num_addresses = 20;

carbon_barrier_t barrier;

IntPtr address = 0x1000;

int main (int argc, char *argv[])
{
   printf("Starting (shared_mem_test4)\n");
   CarbonStartSim(argc, argv);
   CarbonEnableModels();

   CarbonBarrierInit(&barrier, num_threads);

   carbon_thread_t tid_list[num_threads];

   Core* core = Sim()->getTileManager()->getCurrentCore();

   for (int i = 0; i < num_threads-1; i++)
   {
      tid_list[i] = CarbonSpawnThread(thread_func, NULL);
   }
   thread_func(NULL);

   for (int i = 0; i < num_threads-1; i++)
   {
      CarbonJoinThread(tid_list[i]);
   }
  
   printf("shared_mem_test4 (SUCCESS)\n");
  
   CarbonDisableModels();
   CarbonStopSim();
   return 0;
}

void* thread_func(void*)
{
   Core* core = Sim()->getTileManager()->getCurrentCore();

   for (int i = 0; i < num_iterations; i++)
   {
      for (int j = 0; j < num_addresses; j++)
      {
         IntPtr address = IntPtr(j) << 6;
         if (core->getTile()->getId() == 0)
         {
            core->initiateMemoryAccess(MemComponent::L1_DCACHE, Core::NONE, Core::WRITE, address, (Byte*) &i, sizeof(i));
            LOG_PRINT("Core(%i)", core->getTile()->getId());
         }
         
         CarbonBarrierWait(&barrier);

         int val;
         core->initiateMemoryAccess(MemComponent::L1_DCACHE, Core::NONE, Core::READ, address, (Byte*) &val, sizeof(val));
         LOG_PRINT("Core(%i)", core->getTile()->getId());
         if (val != i)
         {
            fprintf(stderr, "shared_mem_test4 (FAILURE): Core(%i), Expected(%i), Got(%i)\n",
                    core->getTile()->getId(), i, val);
            exit(-1);
         }

         CarbonBarrierWait(&barrier);
      } 
   }
   return NULL;
}