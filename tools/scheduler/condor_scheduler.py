#!/usr/bin/env python

from scheduler import Scheduler
from termcolors import *

# Condor Scheduler
class CondorScheduler(Scheduler):
   def __init__(self, jobs, results_dir, config_filename):
      Scheduler.__init__(self, jobs, results_dir, config_filename)

   def start(self):
      # Schedule all the jobs
      for job in self.jobs:
         job.spawn()

   def iterate(self):
      # Poll jobs to see if any of them completed
      # If number of jobs = 0, end the simulation
      if len(self.jobs) == 0:
         return True

      # check active jobs
      terminated = []
      for i in range(0, len(self.jobs)):
         status = self.jobs[i].poll()
         if status != None:
            terminated.append(i)

      terminated.reverse()
      for i in terminated:
         del self.jobs[i]

      return False

   def stop(self):
      # Kill all jobs
      for job in self.jobs:
         msg = colorstr('Keyboard interrupt. Killing simulation', 'RED')
         print "%s: %s" % (msg, job.command)
         job.kill()
