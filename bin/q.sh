#!/usr/bin/bash
export FI_CXI_DEFAULT_CQ_SIZE=131072
export FI_CXI_CQ_FILL_PERCENT=20
export HDF5_VOL_CONNECTOR="daos"
export HDF5_PLUGIN_PATH="/home/hyoklee/lib"
export DAOS_POOL=CSC250STDM10
export LD_LIBRARY_PATH="/home/hyoklee/lib":$LD_LIBRARY_PATH

qsub -v DAOS_POOL=CSC250STDM10,DAOS_CONT=h5 ./j.pbs

