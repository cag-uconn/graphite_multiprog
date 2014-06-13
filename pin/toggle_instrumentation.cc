#include "simulator.h"
#include "pin.H"

void Simulator::enableFrontEnd()
{
   LOG_PRINT("Remove Instrumentation");
   PIN_RemoveInstrumentation();
}

void Simulator::disableFrontEnd()
{
   LOG_PRINT("Remove Instrumentation");
   PIN_RemoveInstrumentation();
}
