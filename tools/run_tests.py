#!/usr/bin/env python

import sys
import os

sys.path.append("./tools/")
sys.path.append("./tools/scheduler")
sys.path.append("./tools/job")

from simulate import *
from benchmark_config import *
from utils import *
from sim_job import SimJob

def getAppFlags(benchmark):
   app_flags = None
   if benchmark in app_flags_table:
      app_flags = app_flags_table[benchmark]
   return app_flags

# scheduler: Use 'condor' for the condor scheduling system or 'basic' for using Graphite's scheduling system
scheduler = "basic"

# results_dir: Directory where the simulation results are placed
results_dir = "./results/regress"
# config_filename: Config file to use for the simulation
config_filename = "carbon_sim.cfg"

# machines: List of machines to run the simulation on
#   Is only used for the 'basic' scheduling system
#   Warning: DO NOT use 'localhost' or '127.0.0.1', use the machine name
machines = [
    "draco1",
    "draco2",
    "draco3",
    "draco4",
    "draco5",
    "draco6",
    ]

# benchmark_list: List of benchmarks to use (now includes SPLASH-2 & PARSEC)
#benchmark_list = splash2_list + parsec_list
benchmark_list = splash2_list

# First, build all the benchmarks
compileBenchmarks(benchmark_list)

# Generate jobs
jobs = []
for benchmark in benchmark_list:
   # Generate command
   command = getCommand(benchmark)

   # Get APP_FLAGS
   app_flags = getAppFlags(benchmark)

   # Generate SIM_FLAGS
   sim_flags = "--general/total_cores=64 " + \
               "--general/mode=lite " + \
               "--general/enable_power_modeling=true " + \
               "--general/trigger_models_within_application=true "
      
   # Generate sub_dir where results are going to be placed
   sub_dir = "%s" % (benchmark)
  
   # Append to jobs list
   jobs.append(SimJob(command, 1, config_filename, results_dir, sub_dir, sim_flags, app_flags, "pin", scheduler))

# Go!
simulate(scheduler, jobs, machines, results_dir, config_filename)
