#!/usr/bin/env python

import os
import shutil

from job import Job

class MasterJob(Job):
   def __init__(self, command, output_dir, config_filename, batch_job):
      Job.__init__(self, command, self.getGraphiteHome())
      self.output_dir = output_dir
      self.config_filename = config_filename
      self.batch_job = batch_job
      # Create output directory
      self.createOutputDir()

   def spawn(self, proc_num):
      return Job.spawn(self, proc_num)

   # get GRAPHITE_HOME from environment variable, or use pwd
   def getGraphiteHome(self):
      graphite_home = os.environ.get('GRAPHITE_HOME')
      if graphite_home == None:
         cwd = os.getcwd()
         warning_msg = "GRAPHITE_HOME undefined. Setting GRAPHITE_HOME to %s" % (cwd)
         print "\n%s" % (warning_msg)
         return cwd
      return graphite_home

   def createOutputDir(self):
      if self.batch_job == "false":
         try:
            os.mkdir(self.output_dir)
         except OSError:
            pass

         try:
            os.remove("%s/results/latest" % (self.graphite_home))
         except OSError:
            pass

         os.symlink(self.output_dir, "%s/results/latest" % (self.graphite_home))

      os.system("echo \"%s\" > %s/command" % (self.command, self.output_dir))
      shutil.copyfile(self.config_filename, "%s/carbon_sim.cfg" % (self.output_dir))
