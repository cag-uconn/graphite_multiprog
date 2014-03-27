#!/usr/bin/env python

import sys
import os

sys.path.append("./tools/")
sys.path.append("./tools/scheduler")
sys.path.append("./tools/job")

from simulate import *
from config import *
from utils import *
from sim_job import SimJob

def getSimulationMode(benchmark):
   if benchmark in lite_mode_list:
      return "lite"
   else: # Works in full & lite modes
      return "full"

# Compile benchmarks
compileBenchmarks(benchmark_list)

# Generate jobs
jobs = []

for benchmark in benchmark_list:
   # Generate command
   command = getCommand(benchmark)

   # Work in lite/full mode?
   simulation_mode = getSimulationMode(benchmark)

   for num_machines in num_machines_list:
      # Don't schedule the benchmarks that work only in lite mode on multiple machines
      if (simulation_mode == "lite") and (num_machines > 1):
         continue

      # Generate SIM_FLAGS
      sim_flags = "--general/total_cores=64 " + \
                  "--general/mode=%s " % (simulation_mode) + \
                  "--general/trigger_models_within_application=true"

      # Generate sub_dir where results are going to be placed
      sub_dir = "%s--procs-%i" % (benchmark, num_machines)

      jobs.append(SimJob(command, num_machines, config_filename, results_dir, sub_dir, sim_flags, None, "pin", scheduler))

try:
   # Remove the results directory
   shutil.rmtree(results_dir)
   # Create results directory
   os.makedirs(results_dir)
except OSError:
   pass
   
# Go!
simulate(scheduler, jobs, machines, results_dir, config_filename)
