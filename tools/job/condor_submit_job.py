#!/usr/bin/env python

import commands
import re

import condor_config
from master_job import *

class CondorSubmitJob:
   def __init__(self, results_dir, output_dir_list):
      self.results_dir = results_dir
      self.createSubmitScript(output_dir_list)

   def createSubmitScript(self, output_dir_list):
      submit_script = "GetEnv = True\n" + \
                      "Universe = vanilla\n" + \
                      "Notification = Error\n" + \
                      "should_transfer_files = IF_NEEDED\n" + \
                      "WhenToTransferOutput = ON_EXIT\n" + \
                      "Executable = /bin/bash\n" + \
                      "Log = /tmp/echo.$ENV(USER).log\n" + \
                      "RequestCpus = %s\n" % (condor_config.request_cpus) + \
                      "RequestMemory = %s\n" % (condor_config.request_memory) + \
                      "Requirements = %s\n" % (condor_config.requirements) + \
                      "Rank = %s\n\n" % (condor_config.rank)

      for output_dir in output_dir_list:
         submit_script = submit_script + \
                         "Arguments = %s/condor_job.sh\n" % (output_dir) + \
                         "Error = %s/condor_job.output\n" % (output_dir) + \
                         "Output = %s/condor_job.output\n" % (output_dir) + \
                         "Queue\n\n"

      script_file = open("%s/condor_job.submit" % (self.results_dir), 'w')
      script_file.write(submit_script)
      script_file.close()

   def spawn(self):
      # Get cluster ID corresponding to Condor job
      output = commands.getoutput("condor_submit %s/condor_job.submit" % (self.results_dir))
      cluster_match = re.search(r'submitted to cluster ([0-9]+)', output)
      assert(cluster_match != None)
      self.cluster_id = cluster_match.group(1)
      print "Condor cluster: %s started" % (self.cluster_id)

   def poll(self):
      # Check condor queue to see if the process is still active
      running_clusters = commands.getoutput("condor_q -format \'%s,\' ClusterId").split(',')
      return (self.cluster_id in running_clusters)

   def wait(self):
      # Check condor queue to see if the process is still active
      while True:
         ret = self.poll()
         if ret == False:
            if self.batch_job == "false":
               os.system("cat %s/condor_job.output" % (self.results_dir))
            exit_code = commands.getoutput("condor_history %s -long | grep ExitCode | cut -f 3 -d \' \'" % (self.cluster_id))
            assert(exit_code != "")
            return int(exit_code)
      time.sleep(0.5)

   def kill(self):
      os.system("condor_rm %s" % (self.cluster_id))
