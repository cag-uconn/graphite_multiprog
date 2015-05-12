#!/usr/bin/env python

from basic_scheduler import BasicScheduler
#from condor_scheduler import CondorScheduler

def createScheduler(scheduler, jobs, machines, results_dir, config_filename):
   if (scheduler == "basic"):
      return BasicScheduler(jobs, machines, results_dir, config_filename)
#   elif (scheduler == "condor"):
#      return CondorScheduler(jobs, results_dir, config_filename)
   else:
      print "*ERROR* Unrecognized Scheduler: %s" % (scheduler)
      sys.exit(4)

def simulate(scheduler, jobs, machines, results_dir, config_filename):
   createScheduler(scheduler, jobs, machines, results_dir, config_filename).run()
 