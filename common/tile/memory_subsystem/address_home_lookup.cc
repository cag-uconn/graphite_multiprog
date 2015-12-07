#include "address_home_lookup.h"
#include "log.h"
#include "config.h"
#include "simulator.h"

DramAddressHomeLookup::DramAddressHomeLookup(UInt32 dramahl_param, vector<tile_id_t>& dramtile_list, UInt32 dramcache_line_size):
   dram_ahl_param(dramahl_param),
   dram_tile_list(dramtile_list),
   dram_cache_line_size(dramcache_line_size)
{
  //string controller_partitioning_type;
  //try
  //{
  //    controller_partitioning_type=Sim()->getCfg()->getString("controller_static_partitioning/type");
  //}
  //catch (...)
  //{
  //   LOG_PRINT_ERROR("Error reading the static memory controller partitioning scheme");
  //}    
   LOG_ASSERT_ERROR((1 << dram_ahl_param) >= (SInt32) dram_cache_line_size,
                    "[1 << AHL param](%u) must be >= [Cache Block Size](%u)",
                    1 << dram_ahl_param, dram_cache_line_size);
   dram_total_modules = dramtile_list.size();
   LOG_PRINT("tile_list size %i", dram_tile_list.size());
   
   //SInt32 drammodule_num_final;
}

DramAddressHomeLookup::~DramAddressHomeLookup()
{}


tile_id_t
DramAddressHomeLookup::getDramHome(IntPtr dramaddress) const
{
  //SInt32 drammodule_num_final=3;
  // if (controller_partitioning_type == "no_part")
 // {
     SInt32 drammodule_num_final; 
     drammodule_num_final = (dramaddress >> dram_ahl_param) % dram_total_modules;
     LOG_ASSERT_ERROR(0 <= drammodule_num_final && drammodule_num_final < (SInt32) dram_total_modules, "drammodule_num(%i), dramtotal_modules(%u)",drammodule_num_final, dram_total_modules);
      LOG_PRINT("nopart dram_module_num(%a),dram_total_modules(%i),dram_ahl_param(%i),dramaddress(%i)",drammodule_num_final,dram_total_modules,dram_ahl_param,dramaddress); 
  //LOG_PRINT("dram_address(%#lx), dram_module_num(%i)", dram_address, dram_module_num);
   
      return (dram_tile_list[drammodule_num_final]);
   }
    //drammodule_num_final=drammodule_num;    
  //




/*tile_id_t
DramAddressHomeLookup::getDramHome(IntPtr dramaddress) const
 // else if (controller_partitioning_type == "part13") 
{  
   SInt32 dramtarget_id = dramaddress >> 48;
   
   SInt32 drammodule_num_list_0[1] = {0};
   SInt32 drammodule_num_list_1[3] = {1,2,3};

   SInt32 drammodule_num = (dramaddress >> dram_ahl_param) % dram_total_modules;
   SInt32 drammodule_num_target;
   SInt32 drammodule_num_final;
 
   if(dramtarget_id == 0)
   {
     drammodule_num_target = drammodule_num % 1;
     drammodule_num_final = drammodule_num_list_0[drammodule_num_target]; 
   }
   else if(dramtarget_id != 0)

   {
     drammodule_num_target = drammodule_num % 3;
     drammodule_num_final = drammodule_num_list_1[drammodule_num_target]; 
   } 

   LOG_ASSERT_ERROR(0 <= drammodule_num && drammodule_num < (SInt32) dram_total_modules, "drammodule_num(%i), dramtotal_modules(%u)", drammodule_num, dram_total_modules);
   
   LOG_PRINT("part13 dramaddress(%#lx), dramtarget_id(%i), drammodule_num(%i), drammodule_num_target(%i), drammodule_num_final(%i), dramtile_id(%i)", dramaddress, dramtarget_id, drammodule_num, drammodule_num_target, drammodule_num_final, dram_tile_list[drammodule_num_final]);
  
   return (dram_tile_list[drammodule_num_final]);
  }*/



//   if (dram_total_modules == 1)
//      return 0;
//   else*/
   //}
   //else if (controller_partitioning_type == "part31") 






/*tile_id_t
DramAddressHomeLookup::getDramHome(IntPtr dramaddress) const
{
   //SInt32 module_num_final;

   SInt32 dramtarget_id = dramaddress >> 48;
   
   SInt32 drammodule_num_list_0[3] = {0,1,2};
   SInt32 drammodule_num_list_1[1] = {3};

   SInt32 drammodule_num = (dramaddress >> dram_ahl_param) % dram_total_modules;
   SInt32 drammodule_num_target;
   SInt32 drammodule_num_final;
 
   if(dramtarget_id == 0)
   {
     drammodule_num_target = drammodule_num % 3;
     drammodule_num_final = drammodule_num_list_0[drammodule_num_target]; 
   }
   else if(dramtarget_id != 0)

   {
     drammodule_num_target = drammodule_num % 1;
     drammodule_num_final = drammodule_num_list_1[drammodule_num_target]; 
   } 

   LOG_ASSERT_ERROR(0 <= drammodule_num && drammodule_num < (SInt32) dram_total_modules, "drammodule_num(%i), dramtotal_modules(%u)", drammodule_num, dram_total_modules);
   
   LOG_PRINT("part31 dramaddress(%#lx), dramtarget_id(%i), drammodule_num(%i), drammodule_num_target(%i), drammodule_num_final(%i), dramtile_id(%i)", dramaddress, dramtarget_id, drammodule_num, drammodule_num_target, drammodule_num_final, dram_tile_list[drammodule_num_final]);
   return (dram_tile_list[drammodule_num]);
}*/
//   if (dram_total_modules == 1)
//      return 0;
//   else
//   }
//  else if (controller_partitioning_type == "equal")



/*tile_id_t
DramAddressHomeLookup::getDramHome(IntPtr dramaddress) const
 
  {  
   SInt32 dramtarget_id = dramaddress >> 48;
   
   SInt32 drammodule_num_list_0[2] = {0,2};
   SInt32 drammodule_num_list_1[2] = {1,3};

   SInt32 drammodule_num = (dramaddress >> dram_ahl_param) % dram_total_modules;
   SInt32 drammodule_num_target;
   SInt32 drammodule_num_final;
 
   if(dramtarget_id == 0)
   {
     drammodule_num_target = drammodule_num % 2;
     drammodule_num_final = drammodule_num_list_0[drammodule_num_target]; 
   }
   else if(dramtarget_id != 0)

   {
     drammodule_num_target = drammodule_num % 2;
     drammodule_num_final = drammodule_num_list_1[drammodule_num_target]; 
   } 

   LOG_ASSERT_ERROR(0 <= drammodule_num && drammodule_num < (SInt32) dram_total_modules, "drammodule_num(%i), dramtotal_modules(%u)", drammodule_num, dram_total_modules);
   
   LOG_PRINT("equal dramaddress(%#lx), dramtarget_id(%i), drammodule_num(%i), drammodule_num_target(%i), drammodule_num_final(%i), dramtile_id(%i)", dramaddress, dramtarget_id, drammodule_num, drammodule_num_target, drammodule_num_final, dram_tile_list[drammodule_num_final]);
   return (dram_tile_list[drammodule_num]);
//   if (dram_total_modules == 1)
//      return 0;
//   else
   }*/


//   return (dram_tile_list[drammodule_num_final]);   TO_BE_PARAMETERIZED

AddressHomeLookup::AddressHomeLookup(UInt32 ahl_param, vector<tile_id_t>& tile_list, UInt32 cache_line_size):
   _ahl_param(ahl_param),
   _tile_list(tile_list),
   _cache_line_size(cache_line_size)
{
   
  
  
  LOG_ASSERT_ERROR((1 << _ahl_param) >= (SInt32) _cache_line_size,"[1 << AHL param](%u) must be >= [Cache Block Size](%u)",1 << _ahl_param, _cache_line_size);
   _total_modules = tile_list.size();
   LOG_PRINT("tile_list size %i", _tile_list.size());
}

AddressHomeLookup::~AddressHomeLookup()
{}


 tile_id_t
AddressHomeLookup::getHome(IntPtr address) const
{
    
 
 
// if (l2slice_partitioning_type == "no_part") 
//   { 
   SInt32 module_num_final;
 
  module_num_final = (address >> _ahl_param) % _total_modules;
   LOG_ASSERT_ERROR(0 <= module_num_final && module_num_final < (SInt32) _total_modules, "module_num(%i), total_modules(%u)", module_num_final, _total_modules);
   LOG_PRINT("nopart module_num_final(%i),total_modules(%i),_ahl_param(%i),address(%i)",module_num_final,_total_modules,_ahl_param,address);  //LOG_PRINT("address(%#lx), module_num(%i)", address, module_num);

  //if (_total_modules == 1)
  //    return 0;
  return (_tile_list[module_num_final]);
 }




/*tile_id_t
AddressHomeLookup::getHome(IntPtr address) const
{
   SInt32 target_id = address >> 48;
   
   SInt32 module_num_list_0[2] = {0,2};
   SInt32 module_num_list_1[2] = {1,3};

   SInt32 module_num = (address >> _ahl_param) % _total_modules;
   SInt32 module_num_target;
   SInt32 module_num_final;
 
   if(target_id == 0)
   {
     module_num_target = module_num % 2;
     module_num_final = module_num_list_0[module_num_target]; 
   }
   else
   {
     module_num_target = module_num % 2;
     module_num_final = module_num_list_1[module_num_target]; 
   } 

   LOG_ASSERT_ERROR(0 <= module_num && module_num < (SInt32) _total_modules, "module_num(%i), total_modules(%u)", module_num, _total_modules);
   
   LOG_PRINT("equal address(%#lx), target_id(%i), module_num(%i), module_num_target(%i), module_num_final(%i), tile_id(%i)", address, target_id, module_num, module_num_target, module_num_final, _tile_list[module_num_final]);
//    if (_total_modules == 1)
//      return 0;

   
   return (_tile_list[module_num_final]);
}*/
//   if (_total_modules == 1)
//      return 0;
//   else




/* tile_id_t
 AddressHomeLookup::getHome(IntPtr address) const
// else if(l2slice_partitioning_type=="part31")
{  
   SInt32 target_id = address >> 48;
   
   SInt32 module_num_list_0[3] = {0,1,2};
   SInt32 module_num_list_1[1] = {3};

   SInt32 module_num = (address >> _ahl_param) % _total_modules;
   SInt32 module_num_target;
   SInt32 module_num_final;
 
   if(target_id == 0)
   {
     module_num_target = module_num % 3;
     module_num_final = module_num_list_0[module_num_target]; 
   }
   else
   {
     module_num_target = module_num % 1;
     module_num_final = module_num_list_1[module_num_target]; 
   } 

   LOG_ASSERT_ERROR(0 <= module_num && module_num < (SInt32) _total_modules, "module_num(%i), total_modules(%u)", module_num, _total_modules);
   
   LOG_PRINT("part31 address(%#lx), target_id(%i), module_num(%i), module_num_target(%i), module_num_final(%i), tile_id(%i)", address, target_id, module_num, module_num_target, module_num_final, _tile_list[module_num_final]);
   return (_tile_list[module_num_final]);
//   if (_total_modules == 1)
//      return 0;
//   else
}*/
  




/* tile_id_t
AddressHomeLookup::getHome(IntPtr address) const
 {
   SInt32 module_num_final;
// else if (l2slice_partitioning_type=="part13")
 
    
   SInt32 target_id = address >> 48;
   
   SInt32 module_num_list_0[1] = {0};
   SInt32 module_num_list_1[3] = {1,2,3};

   SInt32 module_num = (address >> _ahl_param) % _total_modules;
   SInt32 module_num_target;
   //SInt32 module_num_final;
 
   if(target_id == 0)
   {
     module_num_target = module_num % 1;
     module_num_final = module_num_list_0[module_num_target]; 
   }
   else
   {
     module_num_target = module_num % 3;
     module_num_final = module_num_list_1[module_num_target]; 
   } 

   LOG_ASSERT_ERROR(0 <= module_num && module_num < (SInt32) _total_modules, "module_num(%i), total_modules(%u)", module_num, _total_modules);
   
   LOG_PRINT("part13 address(%#lx), target_id(%i), module_num(%i), module_num_target(%i), module_num_final(%i), tile_id(%i)", address, target_id, module_num, module_num_target, module_num_final, _tile_list[module_num_final]);
//   return (_tile_list[module_num]);
//   if (_total_modules == 1)
//      return 0;
//   else

LOG_PRINT("check_variable(%i)",module_num_final )
return (_tile_list[module_num_final]);

}*/
