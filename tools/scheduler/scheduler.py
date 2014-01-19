#!/usr/bin/env python

import os
import time
import shutil

class Scheduler:
   def __init__(self, jobs, results_dir, config_filename):
      self.jobs = jobs
      self.results_dir = "%s/%s" % (os.getcwd(), results_dir)
      self.config_filename = "%s/%s" % (os.getcwd(), config_filename)

   def run(self):
      # Create results directory, copy config file
      self.createResultsDir()

      self.start()
      try:
         while True:
            finished = self.iterate()
            if finished == True:
               break;
            time.sleep(0.5)
      except KeyboardInterrupt:
         self.stop()

   # helpers
   def createResultsDir(self):
      try:
         os.mkdir(self.results_dir)
      except OSError:
         pass
      shutil.copyfile(self.config_filename, "%s/carbon_sim.cfg" % (self.results_dir))
