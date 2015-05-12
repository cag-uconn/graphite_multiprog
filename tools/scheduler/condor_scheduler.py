#!/usr/bin/env python

import sys
import time
import multiprocessing

from scheduler import Scheduler
from condor_submit_job import CondorSubmitJob
from termcolors import *

# Condor Scheduler
class CondorScheduler(Scheduler):
   def __init__(self, jobs, results_dir, config_filename):
      Scheduler.__init__(self, jobs, results_dir, config_filename)

   def start(self):
      self.output_dir_list = []
      self.num_spawned = 0
      self.num_joined = 0
      self.num_active = 0
      self.THRESHOLD = multiprocessing.cpu_count()

      while self.num_joined < len(self.jobs):
         self.spawnJob()
         self.waitJob()
      
      # Submit the Condor job
      self.condor_job = CondorSubmitJob(self.results_dir, self.output_dir_list)
      self.condor_job.spawn()

   def spawnJob(self):
      if self.num_active < self.THRESHOLD and self.num_spawned < len(self.jobs):
         job = self.jobs[self.num_spawned]
         job.spawn()
         self.output_dir_list.append(job.output_dir)
         self.num_active += 1
         self.num_spawned += 1

   def waitJob(self):
      if self.num_active == self.THRESHOLD or self.num_spawned == len(self.jobs):
         self.jobs[self.num_joined].wait()
         self.num_active -= 1
         self.num_joined += 1

   def iterate(self):
      # Poll the condor_job to see if it has completed
      status = self.condor_job.poll()
      return status == False

   def stop(self):
      msg = colorstr('Keyboard interrupt. Killing condor jobs', 'RED')
      self.condor_job.kill()
