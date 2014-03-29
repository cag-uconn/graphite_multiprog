#!/usr/bin/env python

import commands
import os
import subprocess

class Job:
   def __init__(self, command, graphite_home):
      self.command = command
      self.graphite_home = graphite_home

   # spawn:
   #  start up a command on one machine
   def spawn(self, proc_num):
      # Set LD_LIBRARY_PATH using PIN_HOME from Makefile.config
      os.environ['LD_LIBRARY_PATH'] =  "%s/intel64/runtime" % self.getPinHome()
      os.environ['CARBON_PROCESS_INDEX'] = "%d" % (proc_num)
      os.environ['GRAPHITE_HOME'] = self.graphite_home
      self.proc = subprocess.Popen(self.command, shell=True, preexec_fn=os.setsid, env=os.environ)
      return self.proc

   # get PIN_HOME from Makefile.config
   def getPinHome(self):
      return commands.getoutput("grep '^\s*PIN_HOME' %s/Makefile.config | sed 's/^\s*PIN_HOME\s*=\s*\(.*\)/\\1/'" \
             % (self.graphite_home))
