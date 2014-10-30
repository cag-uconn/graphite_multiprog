#!/usr/bin/env python

import os

from job import Job

class SlaveJob(Job):
   def __init__(self, proc_num, command, graphite_home):
      Job.__init__(self, command, graphite_home)
      self.proc_num = proc_num
   
   def spawn(self):
      return Job.spawn(self, self.proc_num)

   @staticmethod
   # getGraphiteHome:
   #  get the graphite home directory from the script name
   def getGraphiteHome(script_name):
      return (os.sep).join(script_name.split(os.sep)[:-3])
