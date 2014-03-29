#!/usr/bin/env python

from benchmark_config import *

# scheduler: Use 'condor' for the condor scheduling system or 'basic' for using Graphite's scheduling algorithm
scheduler = "basic"

# results_dir: Directory where the simulation results are placed
results_dir = "./tools/regress/simulation_results"
# config_filename: Config file to use for the simulation
config_filename = "carbon_sim.cfg"

# Do not use 'localhost' or '127.0.0.1', use the machine name
machines = [
    "draco1",
    "draco2",
    "draco3",
    "draco4",
    "draco5",
    "draco6",
    ]

#benchmark_list = splash2_list + parsec_list
benchmark_list = splash2_list

num_machines_list = [1,2]
