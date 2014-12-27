#include "load_speculation_handler.h"
#include "simulator.h"
#include "utils.h"
#include "log.h"

LoadSpeculationHandler*
LoadSpeculationHandler::create()
{
   string violation_detection_scheme;
   config::Config* cfg = Sim()->getCfg();
   try
   {
      violation_detection_scheme = cfg->getString("core/speculation_support/violation_detection_scheme");
   }
   catch (...)
   {
      LOG_PRINT_ERROR("Could not read [core/speculation_support] parameters from the cfg file");
   }

   if (violation_detection_scheme == "none")
      return new EmptySpeculationHandler();
   else
      LOG_PRINT_ERROR("Unrecognized Violation Detection Scheme: %s",
                      violation_detection_scheme.c_str());
   return (LoadSpeculationHandler*) NULL;
}
