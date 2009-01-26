#include "core.h"
#include "chip.h"

#include "network.h"
#include "ocache.h"
#include "syscall_model.h"
#include "sync_client.h"
#include "network_types.h"
#include "memory_manager.h"

#include "log.h"
#define LOG_DEFAULT_RANK   core_tid
#define LOG_DEFAULT_MODULE CORE

// externally defined vars

extern LEVEL_BASE::KNOB<Boolean> g_knob_enable_performance_modeling;
extern LEVEL_BASE::KNOB<Boolean> g_knob_enable_dcache_modeling;
extern LEVEL_BASE::KNOB<Boolean> g_knob_enable_icache_modeling;

extern LEVEL_BASE::KNOB<UInt32> g_knob_cache_size;
extern LEVEL_BASE::KNOB<UInt32> g_knob_line_size;
extern LEVEL_BASE::KNOB<UInt32> g_knob_associativity;
extern LEVEL_BASE::KNOB<UInt32> g_knob_mutation_interval;
extern LEVEL_BASE::KNOB<UInt32> g_knob_dcache_threshold_hit;
extern LEVEL_BASE::KNOB<UInt32> g_knob_dcache_threshold_miss;
extern LEVEL_BASE::KNOB<UInt32> g_knob_dcache_size;
extern LEVEL_BASE::KNOB<UInt32> g_knob_dcache_associativity;
extern LEVEL_BASE::KNOB<UInt32> g_knob_dcache_max_search_depth;
extern LEVEL_BASE::KNOB<UInt32> g_knob_icache_threshold_hit;
extern LEVEL_BASE::KNOB<UInt32> g_knob_icache_threshold_miss;
extern LEVEL_BASE::KNOB<UInt32> g_knob_icache_size;
extern LEVEL_BASE::KNOB<UInt32> g_knob_icache_associativity;
extern LEVEL_BASE::KNOB<UInt32> g_knob_icache_max_search_depth; 


using namespace std;

int Core::coreInit(int tid, int num_mod)
{
   core_tid = tid;
   core_num_mod = num_mod;

   network = new Network(this);

   if ( g_knob_enable_performance_modeling ) 
   {
      perf_model = new PerfModel("performance modeler");
      LOG_PRINT("Instantiated performance model.");
   } else 
   {
      perf_model = (PerfModel *) NULL;    
   }   

   if ( g_knob_enable_dcache_modeling || g_knob_enable_icache_modeling ) 
   {
		ocache = new OCache("organic cache", 
                          g_knob_cache_size.Value() * k_KILO,
                          g_knob_line_size.Value(),
                          g_knob_associativity.Value(),
                          g_knob_mutation_interval.Value(),
                          g_knob_dcache_threshold_hit.Value(),
                          g_knob_dcache_threshold_miss.Value(),
                          g_knob_dcache_size.Value() * k_KILO,
                          g_knob_dcache_associativity.Value(),
                          g_knob_dcache_max_search_depth.Value(),
                          g_knob_icache_threshold_hit.Value(),
                          g_knob_icache_threshold_miss.Value(),
                          g_knob_icache_size.Value() * k_KILO,
                          g_knob_icache_associativity.Value(),
                          g_knob_icache_max_search_depth.Value());                        

     	LOG_PRINT("instantiated organic cache model");
  
	} else 
   {
      ocache = (OCache *) NULL;
   }   

   if ( g_config->isSimulatingSharedMemory() ) {
     
      assert( g_knob_enable_dcache_modeling ); 

      LOG_PRINT("instantiated memory manager model");
      memory_manager = new MemoryManager(this, ocache);

   } else {

      memory_manager = (MemoryManager *) NULL;
      LOG_PRINT("No Memory Manager being used");
   
   }

   syscall_model = new SyscallMdl(network);
//   InitLock(&dcache_lock);
   sync_client = new SyncClient(this);

   return 0;
}

int Core::coreSendW(int sender, int receiver, char *buffer, int size)
{
   // Create a net packet
   NetPacket packet;
   packet.sender= sender;
   packet.receiver= receiver;
   packet.type = USER;
   packet.length = size;
   packet.data = buffer;

   /*
   packet.data = new char[size];
   for(int i = 0; i < size; i++)
      packet.data[i] = buffer[i];
   */

   SInt32 sent = network->netSend(packet);

   assert(sent == size);

   return sent == size ? 0 : -1;
}

int Core::coreRecvW(int sender, int receiver, char *buffer, int size)
{
   NetPacket packet;

   packet = network->netRecv(sender, USER);

   LOG_PRINT("Got packet: from %i, to %i, type %i, len %i", packet.sender, packet.receiver, (SInt32)packet.type, packet.length);

   assert((unsigned)size == packet.length);

   memcpy(buffer, packet.data, size);

   // De-allocate dynamic memory
   // Is this the best place to de-allocate packet.data ?? 
   delete [] (Byte*)packet.data;

   return (unsigned)size == packet.length ? 0 : -1;
}

void Core::fini(int code, void *v, ostream& out)
{
   if ( g_knob_enable_performance_modeling )
     {
        out << "General:\n";
        perf_model->fini(code, v, out);
        network->outputSummary(out);
     }

   if ( g_knob_enable_dcache_modeling || g_knob_enable_icache_modeling )
      ocache->fini(code,v,out);

   delete sync_client;
   delete syscall_model;
   delete ocache;
   delete perf_model;
   delete network;
}


// organic cache wrappers

bool Core::icacheRunLoadModel(IntPtr i_addr, UInt32 size)
{ return ocache->runICacheLoadModel(i_addr, size).first; }

/*
 * dcacheRunModel (mem_operation_t operation, IntPtr d_addr, char* data_buffer, UInt32 data_size)
 *
 * Arguments:
 *   d_addr :: address of location we want to access (read or write)
 *   shmem_req_t :: READ or WRITE
 *   data_buffer :: buffer holding data for WRITE or buffer which must be written on a READ
 *   data_size :: size of data we must read/write
 *
 * Return Value:
 *   hit :: Say whether there has been at least one cache hit or not
 */
bool Core::dcacheRunModel(mem_operation_t operation, IntPtr d_addr, char* data_buffer, UInt32 data_size)
{
	shmem_req_t shmem_operation;
	
	if (operation == LOAD) {
		shmem_operation = READ;
	}
	else {
		shmem_operation = WRITE;
	}

	if (g_config->isSimulatingSharedMemory()) {
                LOG_PRINT("%s - ADDR: %x, data_size: %u, END!!", ((operation==LOAD) ? " READ " : " WRITE "), d_addr, data_size);

		bool all_hits = true;

		if (data_size <= 0) {
			return (true);
			// TODO: this is going to affect the statistics even though no shared_mem action is taking place
		}

		IntPtr begin_addr = d_addr;
		IntPtr end_addr = d_addr + data_size;
	  	IntPtr begin_addr_aligned = begin_addr - (begin_addr % ocache->dCacheLineSize());
		IntPtr end_addr_aligned = end_addr - (end_addr % ocache->dCacheLineSize());
		char *curr_data_buffer_head = data_buffer;

		//TODO set the size parameter correctly, based on the size of the data buffer
		//TODO does this spill over to another line? should shared_mem test look at other DRAM entries?
		for (IntPtr curr_addr_aligned = begin_addr_aligned ; curr_addr_aligned <= end_addr_aligned /* Note <= */; curr_addr_aligned += ocache->dCacheLineSize())
		{
			// Access the cache one line at a time
			UInt32 curr_offset;
			UInt32 curr_size;

			// Determine the offset
			// TODO fix curr_size calculations
			// FIXME: Check if all this is correct
			if (curr_addr_aligned == begin_addr_aligned) {
				curr_offset = begin_addr % ocache->dCacheLineSize();
			}
			else {
				curr_offset = 0;
			}

			// Determine the size
			if (curr_addr_aligned == end_addr_aligned) {
				curr_size = (end_addr % ocache->dCacheLineSize()) - (curr_offset);
				if (curr_size == 0) {
					continue;
				}
			}
			else {
				curr_size = ocache->dCacheLineSize() - (curr_offset);
			}
         
                        LOG_PRINT("Start InitiateSharedMemReq: ADDR: %x, offset: %u, curr_size: %u", curr_addr_aligned, curr_offset, curr_size);

			if (!memory_manager->initiateSharedMemReq(shmem_operation, curr_addr_aligned, curr_offset, curr_data_buffer_head, curr_size)) {
				// If it is a LOAD operation, 'initiateSharedMemReq' causes curr_data_buffer_head to be automatically filled in
				// If it is a STORE operation, 'initiateSharedMemReq' reads the data from curr_data_buffer_head
				all_hits = false;
			}
			
                        LOG_PRINT("End InitiateSharedMemReq: ADDR: %x, offset: %u, curr_size: %u", curr_addr_aligned, curr_offset, curr_size);

			// Increment the buffer head
			curr_data_buffer_head += curr_size;
		}

                LOG_PRINT("%s - ADDR: %x, data_size: %u, END!!", ((operation==LOAD) ? " READ " : " WRITE "), d_addr, data_size);
		
		return all_hits;		    
   
	} 
	else 
	{
	   // run this if we aren't using shared_memory
		// FIXME: I am not sure this is right
		// What if the initial data for this address is in some other core's DRAM (which is on some other host machine)
		if(operation == LOAD)
			return ocache->runDCacheLoadModel(d_addr, data_size).first;
		else
			return ocache->runDCacheStoreModel(d_addr, data_size).first;
   }
}

void Core::debugSetCacheState(IntPtr address, CacheState::cstate_t cstate, char *c_data)
{
	memory_manager->debugSetCacheState(address, cstate, c_data);
}

bool Core::debugAssertCacheState(IntPtr address, CacheState::cstate_t expected_cstate, char *expected_data)
{
	return memory_manager->debugAssertCacheState(address, expected_cstate, expected_data);
}

void Core::debugSetDramState(IntPtr address, DramDirectoryEntry::dstate_t dstate, vector<UInt32> sharers_list, char *d_data)
{
	memory_manager->debugSetDramState(address, dstate, sharers_list, d_data);		   
}

bool Core::debugAssertDramState(IntPtr address, DramDirectoryEntry::dstate_t dstate, vector<UInt32> sharers_list, char *d_data)
{
	return memory_manager->debugAssertDramState(address, dstate, sharers_list, d_data);
}

