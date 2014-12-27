#pragma once

#include <iostream>
#include <string>
using std::ostream;
using std::string;
using std::endl;

#include "time_types.h"
#include "dynamic_memory_info.h"

class LoadSpeculationHandler
{
public:
   LoadSpeculationHandler() { }
   virtual ~LoadSpeculationHandler() { }
   
   virtual void outputSummary(ostream& out)
   { }
   virtual bool check(const Time& load_latency, const DynamicMemoryInfo& info)
   { return false; }
   
   static LoadSpeculationHandler* create();
};

class EmptySpeculationHandler : public LoadSpeculationHandler
{
};
