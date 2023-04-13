#!/usr/bin/bash

. /etc/profile
. /home/hyoklee/.bashrc

echo "Hello" > /home/hyoklee/bin/hello.txt
module load daos/base
module load spack
module load cmake

cd /home/hyoklee/hdf5
/home/hyoklee/bin/ckrev
rc_h5=$?

if [ $rc_h5 -eq 1 ]
then
    make -j install >& /home/hyoklee/hdf5/hdf5_build.log
fi

cd /home/hyoklee/vol-daos
/home/hyoklee/bin/ckrev
rc_vd=$?

if [ $rc_h5 -eq 1 ] || [ $rc_vd -eq 1 ]
then
    cp src/CMakeLists.txt  src/CMakeLists.txt_1.14
    cp src/CMakeLists.txt_1.15  src/CMakeLists.txt
    cd /home/hyoklee/vol-daos/build
    make -j >& /home/hyoklee/vol-daos/vol_daos_build.log
    # Submit test result to my.cdash.org.
    export HDF5_VOL_DAOS_BUILD_CONFIGURATION="RelWithDebInfo"
    export HDF5_VOL_DAOS_ROOT=/home/hyoklee
    # ctest -S /home/hyoklee/vol-daos/test/scripts/sunspot_script.cmake -VV --output-on-failure

    # Submit a parallel job for testing.
    cd /home/hyoklee/src
    ./q.sh

    # Restore source.
    cd /home/hyoklee/vol-daos
    git checkout -- src/CMakeLists.txt
fi


# To measure time
echo "Hello2" > /home/hyoklee/bin/hello2.txt
