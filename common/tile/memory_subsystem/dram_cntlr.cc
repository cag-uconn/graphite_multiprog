#include <cstring>

#include "dram_cntlr.h"
#include "core_model.h"
#include "tile.h"
#include "memory_manager.h"
#include "log.h"
#include "constants.h"
#include "fixed_types.h"

DramCntlr::DramCntlr(Tile* tile,
      float dram_access_cost,
      float dram_bandwidth,
      bool dram_queue_model_enabled,
      string dram_queue_model_type,
      UInt32 cache_line_size)
   : _tile(tile)
   , _cache_line_size(cache_line_size)
{
   _dram_perf_model = new DramPerfModel(dram_access_cost, 
                                        dram_bandwidth,
                                        dram_queue_model_enabled,
                                        dram_queue_model_type,
                                        cache_line_size);

   _dram_access_count = new AccessCountMap[NUM_ACCESS_TYPES];
  
  
  pkt_time_arrival=0;
  request_number=0;
  next_available_slot_0=0;  
  next_available_slot_1=0;
  schedule_time_0=0;
  schedule_time_1=0;
  queue_delay_tp=0;
  next_available_slot_1=0;
  time_schedule_modulo=0; 
  current_address=0;
  pkt_time_arrival=0;
  request_number=0; 
  lag_to_next_slot_1=0; 
  schedule_time=0; 
  current_schedule_time=0;
  previous_schedule_time=0;


}

DramCntlr::~DramCntlr()
{
  printDramAccessCount();
   delete [] _dram_access_count;
  
   delete _dram_perf_model;
}

void
DramCntlr::getDataFromDram(IntPtr address, Byte* data_buf, bool modeled)
{
  //UInt32 previous_address= 0; UInt32 current_address=0; 
  //current_address=address >> 48;
   if (_data_map[address] == NULL)
   {
      _data_map[address] = new(_tile->getId()) Byte[_cache_line_size];
      memset((void*) _data_map[address], 0x00, _cache_line_size);
   }
   memcpy((void*) data_buf, (void*) _data_map[address], _cache_line_size);

   Latency dram_access_latency = modeled ? runDramPerfModel(address) : Latency(0, DRAM_FREQUENCY);
   LOG_PRINT("Dram Access Latency(%llu)", dram_access_latency.getCycles());
  // if(previous_address != current_address)
  // {getShmemPerfModel()->incrCurrTime(dram_access_latency);}
  // else  
 //  {
   //  dram_access_latency=dram_access_latency+dram_access_latency;
   //current_schedule_time=schedule_time;
   //if (request_number == 0)
  // {  
   getShmemPerfModel()->incrCurrTime(dram_access_latency);
 //  }
 //  else if ((request_number != 0) && (current_schedule_time >= previous_schedule_time))
 //  {
 //  getShmemPerfModel()->incrCurrTime(dram_access_latency);
 //  } 
 //  previous_schedule_time=current_schedule_time;
   addToDramAccessCount(address, READ);
  // previous_address=current_address;
}

void
DramCntlr::putDataToDram(IntPtr address, const Byte* data_buf, bool modeled)
{
   LOG_ASSERT_ERROR(_data_map[address] != NULL, "Data Buffer does not exist");
   
   memcpy((void*) _data_map[address], data_buf, _cache_line_size);

   __attribute__((unused)) Latency dram_access_latency = modeled ? runDramPerfModel(address) : Latency(0, DRAM_FREQUENCY);
   
   addToDramAccessCount(address, WRITE);
}

Latency
DramCntlr::runDramPerfModel(IntPtr address)
{

   Time pkt_time = getShmemPerfModel()->getCurrTime();
   current_address=address >> 48;

   //IntPtr address->
 // Temporal partitioning: Each application has a 100ns time window.  
// App 0 has control over the windows (100-200,300-400,500-600,....)
// App 1 has control over the windows (200-300,400-500,600-700,....)
  //current_address=address >> 48;
  /*UInt64 next_available_slot_0;
  UInt64 next_available_slot_1;
  UInt64 request_number;
  UInt64 schedule_time;
  UInt64 schedule_time_0;
  UInt64 schedule_time_1;*/
  pkt_time_arrival = (UInt64) ceil(pkt_time.getTime()/1000.0);
  
  LOG_PRINT("packet_belongs_to_which_target_process?(%llu)", current_address);
  //dividing for nanosecondscale
  LOG_PRINT("packet_arrival_time(%llu)", pkt_time_arrival);
  time_schedule_modulo= pkt_time_arrival % 100;
  LOG_PRINT("packet_arrival_int(%llu)", time_schedule_modulo);
  
  //if(pkt_time_arrival > 200) && (next_available_slot_0) < pkt_time_arrival) && request_number != 0)
 //{
 //  next_available_slot_0 = pkt_time_arrival + 100 - time_schedule_modulo;
 //  next_available_slot_1 = pkt_time_arrival + 200 - time_schedule_modulo;
 //}
if ((request_number == 0) && (current_address==1))
  {
  next_available_slot_0 = 0;
  next_available_slot_1 = 100;
  }
  
  /*next_available_slot_0=next_available_slot_0 + 10 -10;
  } 
  if ((request_number == 0) && (current_address==0))
  {
  next_available_slot_1 = 100;
  next_available_slot_1=next_available_slot_1 + 10 -10;
  } */
  LOG_PRINT("next_available_slot_0(%llu)", next_available_slot_0);
  LOG_PRINT("next_available_slot_1(%llu)", next_available_slot_1);

 
 
  if(current_address==0)
 {
 if(request_number==0 || (pkt_time_arrival > next_available_slot_0)) 
     {
      
       if ((pkt_time_arrival % 200) < 100)
       { 
           schedule_time=pkt_time_arrival + (200-time_schedule_modulo);
           schedule_time_0=schedule_time ; //- time_schedule_modulo;
           schedule_time_0=schedule_time_0 + 10 - 10;
           LOG_PRINT("schedule_time(%llu)", schedule_time);
       }
       else if ((pkt_time_arrival % 200) > 100)
       {
           schedule_time=pkt_time_arrival + (100-time_schedule_modulo);
           schedule_time_0=schedule_time ; //- time_schedule_modulo;
           //schedule_time_0=schedule_time_0 + 10 - 10;
           LOG_PRINT("schedule_time(%llu)", schedule_time);
       }
     }
 else
     {
      schedule_time=next_available_slot_0 ; //+ 100 ; //-time_schedule_modulo);
      schedule_time_0=schedule_time;
      //schedule_time_0=schedule_time_0 + 10 - 10;
      LOG_PRINT("schedule_time(%llu)", schedule_time);
     }
 }
 else if(current_address==1)
 {
 if(request_number==0 || (pkt_time_arrival > next_available_slot_1)) 
     {
      if ((pkt_time_arrival % 200) < 100)
       { 
           schedule_time=pkt_time_arrival + (100-time_schedule_modulo);
           schedule_time_1=schedule_time ; //- time_schedule_modulo;
           //schedule_time_1=schedule_time_1 + 10 - 10;
           LOG_PRINT("schedule_time(%llu)", schedule_time);
       }
       else if ((pkt_time_arrival % 200) > 100)
       {
           schedule_time=pkt_time_arrival + (200-time_schedule_modulo);
           schedule_time_1=schedule_time ; //- time_schedule_modulo;
           //schedule_time_1=schedule_time_1 + 10 - 10;
           LOG_PRINT("schedule_time(%llu)", schedule_time);
           LOG_PRINT("schedule_time(%llu)", schedule_time);
       }
     }
 else
     {
      schedule_time=next_available_slot_1 ; // 200 ; //-time_schedule_modulo);
      schedule_time_1=schedule_time;
      //schedule_time_1=schedule_time_1 + 10 - 10;
      LOG_PRINT("schedule_time(%llu)", schedule_time);
      LOG_PRINT("schedule_time(%llu)", schedule_time);
     }
 }

 if ((request_number == 0) || (schedule_time > previous_schedule_time))
 {
  queue_delay_tp= schedule_time - pkt_time_arrival;
 }
 else
 {
  queue_delay_tp=0;
 }
 previous_schedule_time=schedule_time;

 LOG_PRINT("schedule_time(%lld)", schedule_time);
 LOG_PRINT("pkt_arrival_time(%lld)", pkt_time_arrival);
 LOG_PRINT("queue_delay(%lld)", queue_delay_tp);
 
  //if (request_number > 0)
  //{  
  next_available_slot_0=schedule_time_0+200; 
  LOG_PRINT("next_available_slot_0(%llu)", next_available_slot_0);
  next_available_slot_1=schedule_time_1+200; 
  LOG_PRINT("next_available_slot_1(%llu)", next_available_slot_1);
  LOG_PRINT("schedule_time(%llu)", schedule_time);
  //}
  
  request_number=request_number +1;
  LOG_PRINT("request_number(%llu)", request_number); 
  UInt64 pkt_size = (UInt64) _cache_line_size;
  return _dram_perf_model->getAccessLatency(queue_delay_tp, pkt_size);
   //previous_address=current_address;
}
 
  
 /* if (time_schedule_modulo >=0 && time_schedule_modulo <=99 && current_address==0)
  {
   pkt_time_perf_model = next_available_slot_opening + time_schedule_modulo;
   LOG_PRINT("packet_arrival_tobeperfmodel(%llu)", pkt_time_perf_model);

  } 
  else if (time_schedule_modulo >=0 && time_schedule_modulo <=99 && current_address==1)
  {
   lag_to_next_slot_0=100-time_schedule_modulo; 
   pkt_time_perf_model= next_available_slot_opening +lag_to_next_slot_0;
   LOG_PRINT("packet_arrival_tobeperfmodel(%llu)", pkt_time_perf_model);

  }
  else if (time_schedule_modulo >=100 && time_schedule_modulo <=199 && current_address==0)
  {
   lag_to_next_slot_1=200-time_schedule_modulo; 
   pkt_time_perf_model = next_available_slot_opening +  pkt_time_int+lag_to_next_slot_1;
   LOG_PRINT("packet_arrival_tobeperfmodel(%llu)", pkt_time_perf_model);
  }
  else if (time_schedule_modulo >=100 && time_schedule_modulo <=199 && current_address==1)
  {
   pkt_time_perf_model= next_available_slot_opening + pkt_time_int;
   LOG_PRINT("packet_arrival_tobeperfmodel(%llu)", pkt_time_perf_model);
   }*/
  /*  if(previous_address == current_address)
   { pkt_time_int = (UInt64) ceil(pkt_time.getTime()/1000.0);
     temporal_schedule_calculation=pkt_time_int % 200;
     if ((temporal_schedule_calculation < 1)
     LOG_PRINT("packet_arrival_int(%llu)", pkt_time_int);
     pkt_time_temp = pkt_time_int % 100;
     current_address_lower_bound=pkt_time_int - pkt_time_temp;
     current_address_upper_bound=current_address_lower_bound+100;
     LOG_PRINT("packet_arrival_modulo100(%llu)", pkt_time_temp);
     pkt_time_delayed_temporal=100-pkt_time_temp;
     LOG_PRINT("packet_arrival_tobequeued(%llu)", pkt_time_delayed_temporal);
     next_slot=pkt_time_int + pkt_time_delayed_temporal;
     LOG_PRINT("packet_arrival_tobeperfmodel(%llu)", pkt_time_perf_model);
   }*/
   
/// {getShmemPerfModel()->incrCurrTime(dram_access_latency);}
  // else  
 //  {
   //  dram_access_latency=dram_access_latency+dram_access_latency;


void
DramCntlr::addToDramAccessCount(IntPtr address, AccessType access_type)
{
   _dram_access_count[access_type][address] = _dram_access_count[access_type][address] + 1;
}

void
DramCntlr::printDramAccessCount()
{
   for (UInt32 k = 0; k < NUM_ACCESS_TYPES; k++)
   {
      for (AccessCountMap::iterator i = _dram_access_count[k].begin(); i != _dram_access_count[k].end(); i++)
      {
         if ((*i).second > 100)
         {
            LOG_PRINT("Dram Cntlr(%i), Address(0x%x), Access Count(%llu), Access Type(%s)", 
                  _tile->getId(), (*i).first, (*i).second,
                  (k == READ)? "READ" : "WRITE");
         }
      }
   }
}

ShmemPerfModel*
DramCntlr::getShmemPerfModel()
{
   return _tile->getMemoryManager()->getShmemPerfModel();
}
