#!/usr/bin/env python

import commands
import re

import condor_config
from master_job import *

class CondorMasterJob(MasterJob):
   def __init__(self, command, output_dir, config_filename, batch_job):
      MasterJob.__init__(self, command, output_dir, config_filename, batch_job)
      self.createJobScript()

   def createJobScript(self):
      job_file_contents = "#!/bin/bash\n\n" + \
                          "cd %s; python -u %s/tools/job/condor_slave_job.py %s\n" % (os.getcwd(), self.graphite_home, self.command)
      job_file = open("%s/condor_job.sh" % (self.output_dir), 'w')
      job_file.write(job_file_contents)
      job_file.close()
