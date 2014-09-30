#!/usr/bin/env python

import time

from scheduler import Scheduler
from condor_submit_job import CondorSubmitJob
from termcolors import *

# Condor Scheduler
class CondorScheduler(Scheduler):
   def __init__(self, jobs, results_dir, config_filename):
      Scheduler.__init__(self, jobs, results_dir, config_filename)

   def start(self):
      output_dir_list = []
      for job in self.jobs:
         job.spawn()
         job.wait()
         output_dir_list.append(job.output_dir)
      # Submit the Condor job
      self.condor_job = CondorSubmitJob(self.results_dir, output_dir_list)
      self.condor_job.spawn()

   def iterate(self):
      # Poll the condor_job to see if it has completed
      status = self.condor_job.poll()
      return status == False

   def stop(self):
      msg = colorstr('Keyboard interrupt. Killing condor jobs', 'RED')
      self.condor_job.kill()
