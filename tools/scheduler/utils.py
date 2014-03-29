#!/usr/bin/env python

import os
import sys

from benchmark_config import *

def getCommand(benchmark):
   if benchmark in splash2_list:
      return "make %s_bench_test" % (benchmark)
   elif benchmark in parsec_list:
      return "make %s_parsec" % (benchmark)
   else:
      print "Benchmark: %s not in SPLASH-2 or PARSEC list" % (benchmark)
      sys.exit(-1)

def compileBenchmarks(benchmark_list):
   # Compile all benchmarks first
   for benchmark in benchmark_list:
      if (benchmark in parsec_list) and (not os.path.exists("tests/parsec/parsec-3.0")):
         print "[regress] Creating PARSEC applications directory."
         os.system("make setup_parsec")
      os.system("%s BUILD_MODE=build" % getCommand(benchmark))
