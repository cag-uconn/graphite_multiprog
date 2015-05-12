#!/usr/bin/env python

import sys
import os

sys.path.append("./tools/")

from schedule import *
from config_64 import *

# job info
# Do not use 'localhost' or '127.0.0.1', use the machine name
machines = [
#    "cag1",
    "cag2", 
#    "cag3",
#    "cag4",
#    "cag5",
#    "cag6",
#    "cag7",
#    "cag8",
    ]

results_dir = "./results/parsec_resilience/LP_2_64_checker_ifelse_inside_stores_ondelay_1"

cfg_file = "carbon_sim.cfg"

benchmark_list =  [
#               "patricia",
               "fft",
#               "radix",
#               "lu_contiguous",
#               "lu_non_contiguous",
#               "cholesky",
#               "barnes",
#               "fmm",
#               "ocean_contiguous",
#               "ocean_non_contiguous",
#               "water-nsquared",
#               "water-spatial",
#               "raytrace",
#               "volrend",
#               "blackscholes",
#               "swaptions",
#               "fluidanimate",
#               "canneal",
#               "streamcluster",
               #"dedup",
#               "ferret",
#               "bodytrack",
               #"facesim",
#               "patricia",
#               "static_concomp",
#               "static_community",
#               "matrix_multiply_blocked",
#               "tsp",
#               "susan"
                  ]

                  
                  
#reexe = "true"
#reexe_onoff_en = "false"
resilience_message = "false"
#latency_hiding_en = "false"
instruction_interval = 100
ssb_size = 8
opport_en =  "false"
extra_reexe_delay = 0
extra_reexe_delay_l1miss = 0             

#resilience_setup_list= [("false","false","false"),("true","false","false"),("true","false","true"),("true","true","true"),("true","true","false")] #(reexe,onoff,latencyHiding)
#resilience_setup_list=[("false","false","false")]
resilience_setup_list= [("true","true","true")]

quantum =1000
                  
num_cores = 64
cluster_size = 1 #[1, 4, 16, 64, 256]
#P2R_threshold_list = [1] #[1,3] # 3
P2R_threshold = 1
max_R2P_threshold = 16
num_R2P_threshold_levels = 4

page_table_enabled = "false" #"true"
page_size = 4096
l2_cache_homing_policy = "striped" #"r-nuca"
classifier_type = "L1_replica_locality"
classifier_granularity = "cache_line_level"
num_tracked_sharers = 3
num_tracked_cache_lines = 3
interleave_granularity = 64

# Compile all benchmarks first
for benchmark in benchmark_list:
   if benchmark in parsec_list:
      if (not os.path.exists("tests/parsec/parsec-3.0")):
         print "[regress] Creating PARSEC applications directory."
         os.system("make setup_parsec")
      os.system("make %s_parsec BUILD_MODE=build" % (benchmark))
   else:
      os.system("make %s_bench_test BUILD_MODE=build" % (benchmark))
      
# Generate jobs
jobs = []

for benchmark in benchmark_list:
   # Generate command
   if benchmark in parsec_list:
      command = "make %s_parsec" % (benchmark)
   else:
      command = "make %s_bench_test" % (benchmark)

   # Get APP_FLAGS
   app_flags = None
   if benchmark in app_flags_table:
      app_flags = app_flags_table[benchmark]
      
   print command
   print app_flags

   for resilience_setup in resilience_setup_list:
      sim_flags = "--general/total_cores=%i --general/enable_shared_mem=true " % (num_cores) + \
		  "--page_table/enabled=%s " % (page_table_enabled) + \
                  "--page_table/page_size=%i " % (page_size) + \
                  "--caching_protocol/type=locality_aware_protocol " + \
                  "--caching_protocol/locality_aware_protocol/l2_cache_homing_policy=%s " % (l2_cache_homing_policy) + \
                  "--caching_protocol/locality_aware_protocol/classifier_type=%s " % (classifier_type) + \
                  "--caching_protocol/locality_aware_protocol/classifier_granularity=%s " % (classifier_granularity) + \
                  "--caching_protocol/locality_aware_protocol/num_tracked_sharers=%i " % (num_tracked_sharers) + \
                  "--caching_protocol/locality_aware_protocol/num_tracked_cache_lines=%i " % (num_tracked_cache_lines) + \
                  "--caching_protocol/locality_aware_protocol/cluster_size=%i " % (cluster_size) + \
                  "--caching_protocol/locality_aware_protocol/interleave_granularity=%i " % (interleave_granularity) + \
                  "--caching_protocol/locality_aware_protocol/core/P2R_threshold=%i " % (P2R_threshold) + \
                  "--caching_protocol/locality_aware_protocol/core/max_R2P_threshold=%i " % (max_R2P_threshold) + \
                  "--caching_protocol/locality_aware_protocol/core/num_R2P_threshold_levels=%i " % (num_R2P_threshold_levels) + \
                  "--reexecution/resilient_cc_en=%s " % (resilience_message) + \
                  "--reexecution/latency_hiding_en=%s " % (resilience_setup[2]) + \
                  "--reexecution/instruction_interval=%i " % (instruction_interval) + \
                  "--reexecution/reexe=%s " % (resilience_setup[0]) + \
                  "--reexecution/reexe_onoff_en=%s " % (resilience_setup[1]) + \
                  "--reexecution/ssb_size=%i " % (ssb_size) + \
                  "--reexecution/opport_en=%s " % (opport_en) + \
                  "--reexecution/extra_reexe_delay_l1miss=%i " % (extra_reexe_delay_l1miss) + \
                  "--reexecution/extra_reexe_delay=%i " % (extra_reexe_delay) + \
                  "--clock_skew_management/lax_barrier/quantum=%i " % (quantum)
      sub_dir = "%s--reexe-%s-onoff-%s-latencyHiding-%s" % (benchmark,resilience_setup[0],resilience_setup[1],resilience_setup[2])
      
      print sim_flags
      print sub_dir
      jobs.append(MakeJob(1, command, cfg_file, results_dir, sub_dir, sim_flags, app_flags, "pin"))

# init
try:
    os.makedirs(results_dir)
except OSError:
    pass

shutil.copy(cfg_file, results_dir)

# go!
schedule(machines, jobs)
