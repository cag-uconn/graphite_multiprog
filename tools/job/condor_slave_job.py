#!/usr/bin/env python

import sys

from slave_job import SlaveJob

class CondorSlaveJob(SlaveJob):
   def __init__(self, command, graphite_home):
      SlaveJob.__init__(self, 0, command, graphite_home)

   def spawn(self):
      self.proc = SlaveJob.spawn(self)

   def wait(self):
      return self.proc.wait()

# main -- if this is used as a standalone script
if __name__=="__main__":
   command = " ".join(sys.argv[1:])
   graphite_home = SlaveJob.getGraphiteHome(sys.argv[0])

   job = CondorSlaveJob(command, graphite_home)
   job.spawn()
   sys.exit(job.wait())
