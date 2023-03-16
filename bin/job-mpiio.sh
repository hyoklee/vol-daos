#!/bin/bash
#PBS -A CSC250STDM12_CNDA 
#PBS -l select=2:system=sunspot,place=scatter
#PBS -l walltime=00:10:00
#PBS -N MPIIODAOSEXAMPLE
#PBS -k doe
#PBS -q workq
#
# Test case for MPI-IO code example

export FI_CXI_DEFAULT_CQ_SIZE=131072
export FI_CXI_CQ_FILL_PERCENT=20
export HDF5_VOL_CONNECTOR="daos"
export HDF5_PLUGIN_PATH="/home/hyoklee/lib"
export LD_LIBRARY_PATH="/home/hyoklee/lib":$LD_LIBRARY_PATH
export DAOS_POOL=CSC250STDM10
export DAOS_CONT=h5

export TZ='/usr/share/zoneinfo/US/Central'
export OMP_PROC_BIND=spread
export OMP_NUM_THREADS=8
unset OMP_PLACES

# ranks per node
rpn=4

# threads per rank
threads=1

# nodes per job
nnodes=$(cat $PBS_NODEFILE | wc -l)

# Verify the pool and container are set
if [ -z "$DAOS_POOL" ];
then
    echo "You must set DAOS_POOL"
    exit 1
fi

if [ -z "$DAOS_CONT" ];
then
    echo "You must set DAOS_CONT"
    exit 1
fi

# load daos/base module (if not loaded)
module load daos/base
module unload mpich/50.1/icc-all-pmix-gpu
module use /soft/restricted/CNDA/updates/modulefiles
module load mpich/50.2-daos/icc-all-pmix-gpu

# print your module list (useful for debugging)
module list

# print your environment (useful for debugging)
#env

# turn on output of what is executed
set -x

#
# clean previous mounts (just in case)
#
clean-dfuse.sh ${DAOS_POOL}:${DAOS_CONT}

# launch dfuse on all compute nodes
# will be launched using pdsh
# arguments:
#   pool:container
# may list multiple pool:container arguments
# will be mounted at:
#   /tmp/<pool>/<container>
launch-dfuse.sh ${DAOS_POOL}:${DAOS_CONT}

# change to submission directory
cd $PBS_O_WORKDIR

# run your job(s)

# these test cases assume 'testfile' is in the CWD
cd /tmp/${DAOS_POOL}/${DAOS_CONT}

echo "write"
mpiexec -np $((rpn*nnodes)) \
	-ppn $rpn \
	-d $threads \
	--cpu-bind numa \
	-genvall \
	/soft/daos/examples/src/mpiio-write

echo "read"
mpiexec -np $((rpn*nnodes)) \
	-ppn $rpn \
	-d $threads \
	--cpu-bind numa \
	-genvall \
	/soft/daos/examples/src/mpiio-read

# cleanup dfuse mounts
ls /tmp/${DAOS_POOL}/${DAOS_CONT}

clean-dfuse.sh ${DAOS_POOL}:${DAOS_CONT}

exit 0
