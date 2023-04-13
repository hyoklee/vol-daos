#!/usr/bin/bash
export FI_CXI_DEFAULT_CQ_SIZE=131072
export FI_CXI_CQ_FILL_PERCENT=20
export HDF5_VOL_CONNECTOR="daos"
export HDF5_PLUGIN_PATH="/home/hyoklee/lib"
export DAOS_POOL=CSC250STDM10
export LD_LIBRARY_PATH="/home/hyoklee/lib":$LD_LIBRARY_PATH

## DAOS_CONT is optional.
# qsub -v DAOS_POOL=CSC250STDM10,DAOS_CONT=53e66136-de47-4ad2-a97a-30ffc4287aed ./j.pbs
qsub -v DAOS_POOL=CSC250STDM10 ./j.pbs
# qsub -v DAOS_POOL=CSC250STDM10 ./cron.pbs
