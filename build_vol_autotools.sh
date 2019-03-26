#!/bin/sh        
#
# A script used to first configure and build the HDF5 source distribution
# included with the DAOS VOL connector source code, and then use that built
# HDF5 to build the DAOS VOL connector itself.

# Get the directory of the script itself
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Set the default install directory
INSTALL_DIR="${SCRIPT_DIR}/daos_vol_build"

# Default name of the directory for the included HDF5 source distribution,
# as well as the default directory where it gets installed
HDF5_DIR="hdf5"
HDF5_INSTALL_DIR="${INSTALL_DIR}"
build_hdf5=true

# Directories specifying where components of DAOS have been installed to.
# If any of these are not set, this script should fail.
DAOS_INSTALL_DIR=""
DAOS_LIB_DIRS=""
DAOS_MPI_DIR=""
DAOS_INCLUDES=""
DAOS_LINK=""

# Determine the number of processors to use when
# building in parallel with Autotools make
NPROCS=0

# Compiler flags for linking with UUID
UUID_DIR=""
UUID_LINK="-luuid"

# Compiler flag for linking with the built DAOS VOL
DAOS_VOL_LINK="-ldaosvol"

# Extra compiler options passed to the various steps, such as -Wall
COMP_OPTS="-Wall -pedantic -Wunused-macros -I${SCRIPT_DIR}/src"

# Extra options passed to the DAOS VOL's configure script
DV_OPTS=""


echo
echo "*************************"
echo "* DAOS VOL build script *"
echo "*************************"
echo

usage()
{
    echo "usage: $0 [OPTIONS]"
    echo
    echo "      -h      Print this help message."
    echo
    echo "      -d      Enable debugging output in the DAOS VOL."
    echo
    echo "      -m      Enable memory tracking in the DAOS VOL."
    echo
    echo "      -P DIR  Similar to 'configure --prefix=DIR', specifies"
    echo "              where the DAOS VOL should be installed to. Default"
    echo "              is 'source directory/daos_vol_build'."
    echo
    echo "      -H DIR  To specify a directory where HDF5 has already"
    echo "              been built."
    echo
    echo "      -D DIR  To specify a directory where DAOS has been built."
    echo
    echo "      -U DIR  To specify the top-level directory where the UUID"
    echo "              library is installed, if it was not installed to a"
    echo "              system directory."
    echo
}

optspec=":htdmH:U:P:D:-"
while getopts "$optspec" optchar; do
    case "${optchar}" in
    h)
        usage
        exit 0
        ;;
    d)
        DV_OPTS="${DV_OPTS} --enable-build-mode=debug"
        COMP_OPTS="${COMP_OPTS} -g"
        echo "Enabled DAOS VOL connector debugging"
        echo
        ;;
    m)
        DV_OPTS="${DV_OPTS} --enable-mem-tracking"
        echo "Enabled DAOS VOL connector memory tracking"
        echo
        ;;
    P)
        if [ "$HDF5_INSTALL_DIR" = "$INSTALL_DIR" ]; then
            HDF5_INSTALL_DIR="$OPTARG"
            echo "Set HDF5 install directory to: ${HDF5_INSTALL_DIR}"
        fi

        INSTALL_DIR="$OPTARG"
        echo "Prefix set to: ${INSTALL_DIR}"
        echo
        ;;
    H)
        build_hdf5=false
        HDF5_INSTALL_DIR="$OPTARG"
        DV_OPTS="${DV_OPTS} --with-hdf5=${HDF5_INSTALL_DIR}"
        echo "Set HDF5 install directory to: ${HDF5_INSTALL_DIR}"
        echo
        ;;
    D)
        # Set the base directory where DAOS is installed
        DAOS_INSTALL_DIR="$OPTARG"

        # Set the directory where the DAOS includes should be located at
        if [ -d "${DAOS_INSTALL_DIR}/include" ]; then
            DAOS_INCLUDES="-I${DAOS_INSTALL_DIR}/include"
        else
            echo "DAOS include dir not found; cannot continue"
            exit 1
        fi

        # Set the directory where the CART includes should be located at
        if [ -d "${DAOS_INSTALL_DIR}/include/cart" ]; then
            DAOS_INCLUDES="${DAOS_INCLUDES} -I${DAOS_INSTALL_DIR}/include/cart"
        else
            echo "CART include dir not found; cannot continue"
            exit 1
        fi

        # Set the directory where the DAOS library should be located at
        if [ -d "${DAOS_INSTALL_DIR}/lib" ]; then
            DAOS_LIB_DIRS="${DAOS_INSTALL_DIR}/lib"
            DAOS_LINK="-L${DAOS_INSTALL_DIR}/lib"
        else
            echo "DAOS lib dir not found; cannot continue"
            exit 1
        fi

        # Set the directory where the DAOS version of MPI should be located at
        DAOS_MPI_DIR="${DAOS_INSTALL_DIR}/include"
        export CC="${DAOS_INSTALL_DIR}/bin/mpicc"

        echo "DAOS install dir set to: ${DAOS_INSTALL_DIR}"
        echo
        ;;
    U)
        UUID_DIR="$OPTARG"
        UUID_LINK="-L${UUID_DIR}/lib ${UUID_LINK}"
        DV_OPTS="${DV_OPTS} --with-uuid=${UUID_DIR}"
        echo "UUID lib directory set to: ${UUID_DIR}"
        echo
        ;;
    *)
        if [ "$OPTERR" != 1 ] || case $optspec in :*) ;; *) false; esac; then
            echo "ERROR: non-option argument: '-${OPTARG}'" >&2
            echo
            usage
            echo
            exit 1
        fi
        ;;
    esac
done


# Try to determine a good number of cores to use for parallelizing both builds
if [ "$NPROCS" -eq "0" ]; then
    NPROCS=`getconf _NPROCESSORS_ONLN 2> /dev/null`

    # Handle FreeBSD
    if [ -z "$NPROCS" ]; then
        NPROCS=`getconf NPROCESSORS_ONLN 2> /dev/null`
    fi
fi

# Set up environment
if [ -z "${DAOS_INSTALL_DIR}" ]; then
    echo "DAOS install dir is not set; cannot continue"
    exit 1
fi

if [ -z "${DAOS_LIB_DIRS}" ]; then
    echo "DAOS lib dirs not set; cannot continue"
    exit 1
fi

if [ -z "${DAOS_MPI_DIR}" ]; then
    echo "DAOS MPI dir is not set; cannot continue"
    exit 1
fi

if [ -z "${DAOS_INCLUDES}" ]; then
    echo "DAOS includes not set; cannot continue"
    exit 1
fi

if [ -z "${DAOS_LINK}" ]; then
    echo "DAOS linking is not set; cannot continue"
    exit 1
fi

export LD_LIBRARY_PATH="${DAOS_LIB_DIRS}"
COMP_OPTS="${COMP_OPTS} ${DAOS_INCLUDES} ${DAOS_LINK} -ldaos -ldaos_common -lmpi"

if [ -z "$(ls -A ${SCRIPT_DIR}/${HDF5_DIR})" ]; then
    git submodule init
    git submodule update
fi

# If the user hasn't already, first build HDF5
if [ "$build_hdf5" = true ]; then
    echo "*****************"
    echo "* Building HDF5 *"
    echo "*****************"
    echo
    
    if [ -z "$(ls -A ${SCRIPT_DIR}/${HDF5_DIR})" ]; then
        git submodule init
        git submodule update
    fi

    cd "${SCRIPT_DIR}/${HDF5_DIR}"

    ./autogen.sh || exit 1

    ./configure --prefix="${HDF5_INSTALL_DIR}" --enable-parallel --disable-static CFLAGS="${COMP_OPTS}" || exit 1

    make -j${NPROCS} && make install || exit 1
fi


# Once HDF5 has been built, build the DAOS VOL connector against HDF5.
echo "*******************************************"
echo "* Building DAOS VOL connector and test suite *"
echo "*******************************************"
echo

mkdir -p "${INSTALL_DIR}"

cd "${SCRIPT_DIR}"

./autogen.sh || exit 1

./configure --prefix="${INSTALL_DIR}" ${DV_OPTS} CFLAGS="${COMP_OPTS}" || exit 1

make -j${NPROCS} && make install || exit 1

exit 0