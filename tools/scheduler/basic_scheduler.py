#!/usr/bin/env python

# This takes a list of machines and a list of jobs and greedily runs
# them on the machines as they become available.

from termcolors import *
from scheduler import Scheduler

# schedule - run a list of jobs across some machines
class BasicScheduler(Scheduler):
   def __init__(self, jobs, machines, results_dir, config_filename):
      Scheduler.__init__(self, jobs, results_dir, config_filename)
      self.machines = machines

   def start(self):
      # initialization
      self.available = self.machines
      self.running = []

   def iterate(self):
      # main loop
      # schedule another job (if available)
      self.schedule()

      # If number of running jobs = 0, end the simulation
      if len(self.running) == 0:
         return True

      # Check active jobs
      self.checkActive()
      
      # Batch simulation is not over yet
      return False

   def stop(self):
      # Kill all jobs
      for job in self.running:
         msg = colorstr('Keyboard interrupt. Killing simulation', 'RED')
         #print "%s: %s" % (msg, job.command)
         job.kill()
   
   # helpers
   def schedule(self):
      # Schedule jobs if machines are available
      scheduled = []
      for i in range(0, len(self.jobs)):
         if (self.jobs[i].num_machines <= len(self.available)):
            self.jobs[i].machines = self.available[0:self.jobs[i].num_machines]
            del self.available[0:self.jobs[i].num_machines]
            scheduled.append(i)
    
      scheduled.reverse()
      for i in scheduled:
         job = self.jobs[i]
         del self.jobs[i]
         job.spawn()
         self.running.append(job)

   def checkActive(self):
      # Check active jobs
      terminated = []
      for i in range(0, len(self.running)):
         status = self.running[i].poll()
         if status != None:
            self.available.extend(self.running[i].machines)
            terminated.append(i)

      terminated.reverse()
      for i in terminated:
         del self.running[i]

