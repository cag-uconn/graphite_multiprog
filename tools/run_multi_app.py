#!/usr/bin/env python

import sys
import os

sys.path.append("./tools/")
sys.path.append("./tools/scheduler")
sys.path.append("./tools/job")

from simulate import *
from benchmark_config import *
from utils import *
from sim_job import SimJobMultiApp

# scheduler: Use 'condor' for the condor scheduling system or 'basic' for using Graphite's scheduling system
scheduler = "basic"

# results_dir: Directory where the simulation results are placed
results_dir = "./results/multi_app"
# config_filename: Config file to use for the simulation
config_filename = "carbon_sim.cfg"
config_filename_MultiApp = "multi_app.cfg"

# machines: List of machines to run the simulation on
#   Is only used for the 'basic' scheduling system
#   Warning: DO NOT use 'localhost' or '127.0.0.1', use the machine name
machines = [
    "127.0.0.1",
    "127.0.0.1",
    "127.0.0.1",
    "127.0.0.1",
    "draco5",
    "draco6",
    ]

# benchmark_list: List of benchmarks to use (includes SPLASH-2 & PARSEC)
#   By default, only SPLASH-2 benchmarks are selected
#benchmark_list = splash2_list + parsec_list
benchmark_list = ["fft","radix"]

# First, build all the benchmarks
compileBenchmarks(benchmark_list)

# Generate jobs
jobs = []

#for benchmark in benchmark_list:

# Generate command 
command_MultiApp = [getCommand(benchmark_list[0]), getCommand(benchmark_list[1])]
#command_MultiApp = [getCommand(benchmark_list[0])]

# Get APP_FLAGS
app_flags_MultiApp = [getAppFlags(benchmark_list[0]), getAppFlags(benchmark_list[1])]
#app_flags_MultiApp = [getAppFlags(benchmark_list[0])]


# Generate SIM_FLAGS
sim_flags = "--general/total_cores=64 " + \
            "--general/mode=full " + \
            "--general/enable_power_modeling=true " + \
            "--general/trigger_models_within_application=true "
      
# Generate sub_dir where results are going to be placed
sub_dir = "multi_%s_%s" % (benchmark_list[0], benchmark_list[1])
  
# Append to jobs list
jobs.append(SimJobMultiApp(command_MultiApp, 2, config_filename, config_filename_MultiApp, results_dir, sub_dir, sim_flags, app_flags_MultiApp, "pin", scheduler))

try:
   # Create results directory
   os.makedirs(results_dir)
except OSError:
   pass
   
# Go!
simulate(scheduler, jobs, machines, results_dir, config_filename)
