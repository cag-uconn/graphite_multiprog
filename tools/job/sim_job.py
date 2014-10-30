#!/usr/bin/env python

import sys
import os
import subprocess
import signal
import time

from termcolors import *

# SimJob:
#  a job built around the make system
class SimJob:
   def __init__(self, command, num_machines, config_filename, results_dir, sub_dir, sim_flags, app_flags, mode, scheduler):
      self.command = command
      self.num_machines = num_machines
      self.config_filename = "%s/%s" % (os.getcwd(), config_filename)
      self.output_dir = "%s/%s/%s" % (os.getcwd(), results_dir, sub_dir)
      self.sim_flags = sim_flags
      self.app_flags = app_flags
      self.mode = mode
      self.scheduler = scheduler

   # spawn:
   #  spawn job
   def spawn(self):
      self.makeCommand()
      self.createOutputDir()
      self.proc = subprocess.Popen(self.command, shell=True, preexec_fn=os.setsid)

   # poll:
   #  check if a job has finished
   def poll(self):
      return self.proc.poll()

   # wait:
   #  wait on a job to finish
   def wait(self):
      while True:
         ret = self.proc.poll()
         if ret != None:
            return ret
         time.sleep(0.5)

   # kill:
   #  kill the job
   def kill(self):
      os.killpg(self.proc.pid, signal.SIGINT)
   
   def makeCommand(self):
      self.makeSimFlags()
      self.command += " SIM_FLAGS=\"%s\"" % (self.sim_flags)
      if self.app_flags != None:
         self.command += " APP_FLAGS=\"%s\"" % (self.app_flags)
      self.command += " MODE=\"%s\"" % (self.mode)
      self.command += " SCHEDULER=\"%s\"" % (self.scheduler)
      self.command += " BATCH_JOB=\"true\""
      self.command += " > %s/output 2>&1" % (self.output_dir)
      print self.command

   def makeSimFlags(self):
      self.sim_flags += " -c %s" % (self.config_filename) + \
                        " --general/output_dir=%s" % (self.output_dir) + \
                        " --general/num_processes=%d" % (self.num_machines)
      if (self.scheduler == "basic"):
         for i in range(0,len(self.machines)):
            self.sim_flags += " --process_map/process%d=%s" % (i, self.machines[i])

   def createOutputDir(self):
      try:
         os.mkdir(self.output_dir)
      except OSError:
         pass

