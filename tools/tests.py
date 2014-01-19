#!/usr/bin/env python

import sys
import os

sys.path.append("./tools/")
sys.path.append("./tools/scheduler")
sys.path.append("./tools/job")

from simulate import *
from sim_job import SimJob
from config import *

# Use the 'basic' or 'condor' scheduler
scheduler = "condor"
# Use the 'pin' or 'native' mode
mode = "pin"

# Do not use 'localhost' or '127.0.0.1', use the machine name
machines = [
    "draco1",
#    "draco2",
#    "draco3",
#    "draco4",
#    "draco5",
#    "draco6",
    ]

results_dir = "./results/regress"
config_filename = "carbon_sim.cfg"

benchmark_list = [
                "fft",
                "radix",
                "lu_contiguous",
                "lu_non_contiguous",
                "cholesky",
                "barnes",
                "fmm",
                "ocean_contiguous",
                "ocean_non_contiguous",
                "water-nsquared",
                "water-spatial",
#                "raytrace",
#                "volrend",
#                "blackscholes",
#                "swaptions",
#                "fluidanimate",
#                "canneal",
#                "streamcluster",
#                "dedup",
#                "ferret",
#                "bodytrack",
#                "facesim",
                ]

# First, build all the benchmarks
for benchmark in benchmark_list:
   if benchmark in parsec_list:
      if (not os.path.exists("tests/parsec/parsec-3.0")):
         print "[regress] Creating PARSEC applications directory."
         os.system("make setup_parsec")
      make_cmd = "make %s_parsec BUILD_MODE=build" % (benchmark)
   else: # benchmark in splash2_list
      make_cmd = "make %s_bench_test BUILD_MODE=build" % (benchmark)
   os.system(make_cmd)

# Generate jobs
jobs = []
for benchmark in benchmark_list:
   if benchmark in parsec_list:
      command = "make %s_parsec" % (benchmark)
   else: # benchmark in splash2_list
      command = "make %s_bench_test" % (benchmark)

   # Get APP_FLAGS
   app_flags = None
   if benchmark in app_flags_table:
      app_flags = app_flags_table[benchmark]

   # Generate SIM_FLAGS
   sim_flags = "--general/total_cores=64"
      
   # Generate sub_dir where results are going to be placed
   sub_dir = "%s" % (benchmark)
  
   print command
   print app_flags
   print sim_flags
   print sub_dir

   # Append to jobs list
   jobs.append(SimJob(command, 1, config_filename, results_dir, sub_dir, sim_flags, app_flags, "pin", scheduler))

# go!
simulate(scheduler, jobs, machines, results_dir, config_filename)
