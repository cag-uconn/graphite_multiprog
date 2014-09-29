#!/usr/bin/env python

import sys

# machine_pool: Machine pool to run the Graphite simulations on
# Right now, only choose one of ["fos","draco"]. This will be fixed later.
machine_pool = "draco"

# WARNING: Do not modify anything below this line
if machine_pool == "draco":
   rank = "isDraco"
   requirements = "isDraco"
   request_cpus = "32"
elif machine_pool == "fos":
   rank = "isFos"
   requirements = "isFos"
   request_cpus = "8"
else:
   print "*ERROR* Unrecognized Machine Pool: %s" % (machine_pool)
   sys.exit(-1)
request_memory = "1024"
