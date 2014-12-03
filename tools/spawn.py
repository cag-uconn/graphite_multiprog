#!/usr/bin/env python

"""
This is the client process that distributes n simulators
This is the python script that is responsible for spawning a simulation
"""

import sys
import os
import re
import subprocess
import time
import signal

sys.path.append("%s/tools/job/" % os.environ.get('GRAPHITE_HOME'))
from basic_master_job import BasicMasterJob
from condor_master_job import CondorMasterJob
from condor_submit_job import CondorSubmitJob
from termcolors import *

# Read output_dir from the command string
def getOutputDir(command):
   output_dir_match = re.match(r'.*--general/output_dir\s*=\s*([^\s]+)', command)
   if output_dir_match:
      return output_dir_match.group(1)
   
   print "*ERROR* Could not read output dir"
   sys.exit(-1)

# Read config filename from the command string
def getConfigFilename(command):
   config_filename_match = re.match(r'.*-c\s+([^\s]+\.cfg)\s+', command)
   if config_filename_match:
      return config_filename_match.group(1)
   
   print "*ERROR* Could not read config filename"
   sys.exit(-1)

# Read number of processes (from command string. If not found, from the config file)
def getNumProcesses(command):
   proc_match = re.match(r'.*--general/num_processes\s*=\s*([0-9]+)', command)
   if proc_match:
      return int(proc_match.group(1))
   
   config_filename = getConfigFilename(command)
   config = open(config_filename, 'r').readlines()
   
   found_general = False
   for line in config:
      if found_general == True:
         proc_match = re.match(r'\s*num_processes\s*=\s*([0-9]+)', line)
         if proc_match:
            return int(proc_match.group(1))
      else: 
         if re.match(r'\s*\[general\]', line):
            found_general = True

   print "*ERROR* Could not read number of processes to start the simulation"
   sys.exit(-1)

# Read machine list (from command string. If not found, from the config file)
def getMachineList(command, num_processes):
   machine_list = []

   curr_process_num = 0
   while True:
      hostname_match = re.match(r'.*--process_map/process%d\s*=\s*([A-Za-z0-9.]+)' % (curr_process_num), command)
      if hostname_match:
         machine_list.append(hostname_match.group(1))
         curr_process_num = curr_process_num + 1
         if (curr_process_num == num_processes):
            return machine_list
      else:
         break

   if (curr_process_num > 0):
      print "*ERROR* Found location of at least one process but not all processes from the command string"
      sys.exit(-1)

   
   config_filename = getConfigFilename(command)
   config = open(config_filename).readlines()
   
   found_process_map = False
   for line in config:
      if found_process_map == True:
         hostname_match = re.match(r'\s*process%d\s*=\s*\"([A-Za-z0-9.]+)\"' % (curr_process_num), line)
         if hostname_match:
            machine_list.append(hostname_match.group(1))
            curr_process_num = curr_process_num + 1
            if curr_process_num == num_processes:
               return machine_list
      else: 
         if re.match(r'\s*\[process_map\]', line):
            found_process_map = True

   print "*ERROR* Could not read process list from config file"
   sys.exit(-1)

def getTargetProcessesIndex(command): 
   proc_match = re.match(r'.*--target_process_index\s*=\s*([0-9]+)', command)
   if proc_match:
      return int(proc_match.group(1))

   print "*ERROR* Could not read target process index to start the simulation"
   sys.exit(-1)   
   
def getConfigFilenameMultiApp(command):
   config_filename_match = re.match(r'.*-c\s+([^\s]+\.cfg)\s+', command)
   if config_filename_match:
      return config_filename_match.group(1)
   
   print "*ERROR* Could not read config filename"
   sys.exit(-1)
   
   
   
   
   
# main -- if this is used as standalone script
if __name__=="__main__":
  
   scheduler = sys.argv[1]
   mode = sys.argv[2]
   batch_job = sys.argv[3]
   pin_run = sys.argv[4]
   sim_flags = sys.argv[5]
   exec_command = sys.argv[6]
   config_filename = getConfigFilename(sim_flags)
   output_dir = getOutputDir(sim_flags)
   num_processes = getNumProcesses(sim_flags)
   machine_list = getMachineList(sim_flags, num_processes)
   target_index = getTargetProcessesIndex(sim_flags)

   if (mode == "pin"):
      command = "%s %s -- %s" % (pin_run, sim_flags, exec_command)
   elif (mode == "native"):
      command = "%s %s" % (exec_command, sim_flags)
   else:
      print "*ERROR* Unrecognized Mode: %s" % (mode)
      sys.exit(2)

   if (scheduler == "condor") and (num_processes != 1):
      print "*ERROR* Only single process simulations allowed with condor scheduler"
      sys.exit(3)

   if (scheduler == "basic"):
      job = BasicMasterJob(command, output_dir, config_filename, batch_job, machine_list, target_index)
   elif (scheduler == "condor"):
      cjob = CondorMasterJob(command, output_dir, config_filename, batch_job)
      if (batch_job == "true"):
         sys.exit(0)
      job = CondorSubmitJob(output_dir, [output_dir])
   else:
      print "*ERROR* Unrecognized Scheduler: %s" % (scheduler)
      sys.exit(4)

   # Spawn job
   job.spawn()

   try:
      return_code = job.wait()
      print "%s\n" % (pReturnCode(return_code))
      sys.exit(return_code)
   except KeyboardInterrupt:
      msg = colorstr('Keyboard interrupt. Killing simulation', 'RED')
      print msg
      job.kill()
      sys.exit(signal.SIGINT)

	  