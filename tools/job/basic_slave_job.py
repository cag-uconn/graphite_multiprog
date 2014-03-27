#!/usr/bin/env python

"""
This is the slave process that receives requests over ssh to spawn a simulation
"""

import sys
import os
import signal
import time
import subprocess

from slave_job import SlaveJob
from slave_job import getGraphiteHome

# BasicSlaveJob:
#  a job built around the Graphite scheduler
class BasicSlaveJob(SlaveJob):
   def __init__(self, proc_num, command, working_dir, graphite_home):
      SlaveJob.__init__(self, proc_num, command, graphite_home)
      self.working_dir = working_dir
   
   # spawn:
   #  start up a command over an ssh connection on one machine
   #  returns an object that can be passed to wait()
   def spawn(self):
      os.chdir(self.working_dir)
      print "Starting process: %d: %s" % (self.proc_num, self.command)
      self.proc = SlaveJob.spawn(self)
      self.renew_permissions_proc = self.spawnRenewPermisssionsProc()

   # wait:
   #  wait on a job to finish or the ssh connection to be killed
   def wait(self):
      while True:
         # Poll the process and see if it exited
         return_code = self.proc.poll()
         if return_code != None:
            try:
               os.killpg(self.proc.pid, signal.SIGKILL)
            except OSError:
               pass
            print "Process: %d exited with ReturnCode: %d" % (self.proc_num, return_code)
            if (self.renew_permissions_proc != None):
               os.killpg(self.renew_permissions_proc.pid, signal.SIGKILL)
            return return_code
         
         # If not, check if some the ssh connection has been killed
         # If connection killed, this becomes a child of the init process
         if (os.getppid() == 1):
            # DO NOT place a print statement here
            os.killpg(self.proc.pid, signal.SIGKILL)
            if (self.renew_permissions_proc != None):
               os.killpg(self.renew_permissions_proc.pid, signal.SIGKILL)
            return -1
         
         # Sleep for 0.5 seconds before checking parent pid or process status again
         time.sleep(0.5)

   # spawnRenewPermisssionsProc:
   #  command to renew Kerberos/AFS tokens
   def spawnRenewPermisssionsProc(self):
      try:
        test_proc = subprocess.Popen("krenew")
      except OSError:
        return None
      test_proc.wait()
      return subprocess.Popen("krenew -K 60 -t", shell=True, preexec_fn=os.setsid)
   
# main -- if this is used as a standalone script
if __name__=="__main__":
   proc_num = int(sys.argv[2])
   command = " ".join(sys.argv[3:])
   working_dir = sys.argv[1]
   graphite_home = getGraphiteHome(sys.argv[0])

   job = BasicSlaveJob(proc_num, command, working_dir, graphite_home)
   job.spawn()
   sys.exit(job.wait())
