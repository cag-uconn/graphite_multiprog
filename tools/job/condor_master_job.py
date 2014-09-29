#!/usr/bin/env python

import commands
import re

import condor_config
from master_job import *

# CondorJob:
#  a job built around the condor queueing system
class CondorMasterJob(MasterJob):
   def __init__(self, command, output_dir, config_filename, batch_job):
      MasterJob.__init__(self, command, output_dir, config_filename, batch_job)
   
   def spawn(self):
      self.createScript()
      self.createJob()
      # Get cluster ID corresponding to Condor job
      output = commands.getoutput("condor_submit %s/condor_job.submit" % (self.output_dir))
      cluster_match = re.search(r'submitted to cluster ([0-9]+)', output)
      assert(cluster_match != None)
      self.proc = cluster_match.group(1)

   def poll(self):
      # Check condor queue to see if the process is still active
      running_procs = commands.getoutput("condor_q -format \'%s,\' ClusterId").split(',')
      return (self.proc in running_procs)
      
   def wait(self):
      # Check condor queue to see if the process is still active
      while True:
         ret = self.poll()
         if ret == False:
            if self.batch_job == "false":
               os.system("cat %s/condor_job.output" % (self.output_dir))
            exit_code = commands.getoutput("condor_history %s -long | grep ExitCode | cut -f 3 -d \' \'" % (self.proc))
            assert(exit_code != "")
            return int(exit_code)
      time.sleep(0.5)

   def kill(self):
      os.system("condor_rm %s" % (self.proc))

   def createScript(self):
      script_contents = "GetEnv = True\n" + \
                        "Universe = vanilla\n" + \
                        "Notification = Error\n" + \
                        "should_transfer_files = IF_NEEDED\n" + \
                        "WhenToTransferOutput = ON_EXIT\n" + \
                        "Executable = /bin/bash\n" + \
                        "Arguments = %s/condor_job.sh\n" % (self.output_dir) + \
                        "Log = /tmp/echo.$ENV(USER).log\n" + \
                        "RequestCpus = %s\n" % (condor_config.request_cpus) + \
                        "RequestMemory = %s\n" % (condor_config.request_memory) + \
                        "Requirements = %s\n" % (condor_config.requirements) + \
                        "Rank = %s\n" % (condor_config.rank) + \
                        "Error = %s/condor_job.output\n" % (self.output_dir) + \
                        "Output = %s/condor_job.output\n" % (self.output_dir) + \
                        "queue 1\n"
      script_file = open("%s/condor_job.submit" % (self.output_dir), 'w')
      script_file.write(script_contents)
      script_file.close()

   def createJob(self):
      job_file_contents = "#!/bin/bash\n\n" + \
                          "python -u %s/tools/job/condor_slave_job.py %s\n" % (self.graphite_home, self.command)
      job_file = open("%s/condor_job.sh" % (self.output_dir), 'w')
      job_file.write(job_file_contents)
      job_file.close()
   