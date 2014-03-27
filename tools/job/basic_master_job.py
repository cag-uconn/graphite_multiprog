#!/usr/bin/env python

import os
import time
import signal
import subprocess

from termcolors import *
from master_job import MasterJob

# BasicJob:
#  a job built around the Graphite scheduler
class BasicMasterJob(MasterJob):
   def __init__(self, command, output_dir, config_filename, batch_job, machines):
      MasterJob.__init__(self, command, output_dir, config_filename, batch_job)
      self.working_dir = os.getcwd()
      self.machines = machines
   
   # spawn:
   #  start up a command across multiple machines
   def spawn(self):
      # spawn
      self.procs = {}
      for i in range(0, len(self.machines)):
         if (self.machines[i] == "localhost") or (self.machines[i] == r'127.0.0.1'):
            print "Starting process: %d: %s" % (i, self.command)
            self.procs[i] = MasterJob.spawn(self, i)
         else:
            self.command = self.command.replace("\"", "\\\"")
            slave_command = "python -u %s/tools/job/basic_slave_job.py %s %d \\\"%s\\\"" % \
                            (self.graphite_home, self.working_dir, i, self.command)
            ssh_command = "ssh -x %s \"%s\"" % (self.machines[i], slave_command)
            print "Starting process: %d: %s" % (i, ssh_command)
            self.procs[i] = subprocess.Popen(ssh_command, shell=True, preexec_fn=os.setsid)

   # poll:
   #  check if a job has finished
   #  returns the return_code, or None
   def poll(self):
      # check status
      return_code = None

      for i in range(0,len(self.procs)):
         return_code = self.procs[i].poll()
         if return_code != None:
            break

      # process still running
      if return_code == None:
         return None

      # process terminated, so wait or kill remaining
      for i in range(0,len(self.procs)):
         return_code2 = self.procs[i].poll()
         if return_code2 == None:
            if return_code == 0:
               return_code = self.procs[i].wait()
               print "Process: %d exited with ReturnCode: %d" % (i, return_code)
            else:
               print "Killing process: %d" % (i)
               os.killpg(self.procs[i].pid, signal.SIGKILL)
         else:
            try:
               os.killpg(graphite_procs[i].pid, signal.SIGKILL)
            except OSError:
               pass
            print "Process: %d exited with ReturnCode: %d" % (i, return_code2)

      print "%s\n" % (pReturnCode(return_code))
      return return_code

   # wait:
   #  wait on a job to finish
   def wait(self):
      while True:
         ret = self.poll()
         if ret != None:
            return ret
         time.sleep(0.5)

   # kill:
   #  kill all graphite processes
   def kill(self):
      # Kill graphite processes
      for i in range(0,len(self.procs)):
         return_code = self.procs[i].poll()
         if return_code == None:
            print "Killing process: %d" % (i)
            os.killpg(self.procs[i].pid, signal.SIGKILL)
