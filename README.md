# HDF5 DAOS VOL connector

[![Latest version][vol-daos-release-svg]][vol-daos-release-link]

## Table of Contents
1. [Description](#1-Description)
2. [Installation](#2-Installation)
    * [Prerequisites](#Prerequisites)
    * [Build instructions](#Build-instructions)
    * [CMake options](#CMake-options)
    * [Connector options](#Connector-options)
3. [Testing and Usage](#3-Testing-And-Usage)
4. [More information](#4-More-Information)
    * [DAOS VOL](#DAOS-VOL)
    * [DAOS](#DAOS)

## 1. Description

The HDF5 DAOS VOL connector is a Virtual Object Layer (VOL) connector for HDF5
that allows for direct interfacing with the Distributed Asynchronous Object
Storage (DAOS) system, bypassing both MPI I/O and POSIX for efficient and
scalable I/O, removing the limitations of the native HDF5 file format and
enabling new features such as independent creation of objects in parallel,
key-value store objects, data recovery, asynchronous I/O, etc.

Applications already using HDF5 can, using this VOL connector and a DAOS-enabled
system, benefit of some of these features with minimal code changes.
The connector is built as a plugin library that is external to HDF5, meaning
that it must be dynamically-loaded in the same fashion as HDF5 filter plugins.

## 2. Installation

Below is set a of instructions that is compiled to provide a minimal
installation of the DAOS VOL connector on a DAOS-enabled system.

### Prerequisites

To build the DAOS VOL connector, the following libraries are required:

+ `libhdf5` - The [HDF5](https://www.hdfgroup.org/downloads/hdf5/) library.
            Minimum version required is 1.14.0, compiled with
            support for both parallel I/O and map objects 
            (i.e., `-DHDF5_ENABLE_MAP_API=ON` CMake option).

+ `libdaos` - The [DAOS](https://github.com/daos-stack/daos) library.
            Minimum version required is 1.3.106-tb.

+ `libuuid` - UUID support.

Compiled libraries must either exist in the system's library paths or must be
pointed to during the DAOS VOL connector build process.

### Build instructions

The HDF5 DAOS VOL connector is built using CMake. CMake version 2.8.12.2 or
greater is required for building the connector itself, but version 3.1 or
greater is required to build the connector's tests.

If you install the full sources, put the tarball in a directory where you have permissions (e.g., your home directory) and unpack it:

    gzip -cd hdf5_vol_daos-X.tar.gz | tar xvf -

or

    bzip2 -dc hdf5_vol_daos-X.tar.bz2 | tar xvf -

Replace 'X' with the version number of the package.

After obtaining the connector's source code, you can create a build directory
within the source tree and run the `ccmake` or `cmake` command from it:

    cd hdf5_vol_daos-X
    mkdir build
    cd build
    ccmake ..

If using `ccmake`, type `'c'` multiple times and choose suitable options or if
using `cmake`, pass these options with `-D`. Some of these options may be needed
if, for example, the required components mentioned previously are not located in
default paths.

Setting include directory and library paths may require you to toggle to
the advanced mode by typing `'t'`. Once you are done and do not see any
errors, type `'g'` to generate makefiles. Once you exit the CMake
configuration screen and are ready to build the targets, do:

    make

Verbose build output can be generated by appending `VERBOSE=1` to the
`make` command.

Assuming that the `CMAKE_INSTALL_PREFIX` has been set
and that you have write permissions to the destination directory, you can
install the connector by simply doing:

     make install

### CMake options

  * `CMAKE_INSTALL_PREFIX` - This option controls the install directory that the resulting output files are written to. The default value is `/usr/local`. 
  * `CMAKE_BUILD_TYPE` - This option controls the type of build used for the VOL connector. Valid values are `Release`, `Debug`, `RelWithDebInfo`,
  `MinSizeRel`, `Ubsan`, `Asan`; the default build type is `RelWithDebInfo`.

### Connector options

  * `BUILD_TESTING` - This option is used to enable/disable building of the
  DAOS VOL connector's tests. The default value is `ON`.
  * `BUILD_EXAMPLES` - This option is used to enable/disable building of the
  DAOS VOL connector's HDF5 examples. The default value is `OFF`.
  * `HDF5_C_COMPILER_EXECUTABLE` - This option controls the HDF5 compiler
  wrapper script used by the VOL connector build process. It should be set to
  the full path to the HDF5 compiler wrapper (usually `bin/h5cc`), including
  the name of the wrapper script. The following two options may also need to be
  set.
  * `HDF5_C_LIBRARY_hdf5` - This option controls the HDF5 library used by the
  VOL connector build process. It should be set to the full path to the HDF5
  library, including the library's name (e.g., `/path/libhdf5.so`). Used in
  conjunction with the `HDF5_C_INCLUDE_DIR` option.
  * `HDF5_C_INCLUDE_DIR` - This option controls the HDF5 include directory used
  by the VOL connector build process. Used in conjunction with the
  `HDF5_C_LIBRARY_hdf5` variable.
  * `DAOS_LIBRARY` - This option controls the DAOS library used by the VOL
  connector build process. It should be set to the full path to the DAOS
  library, including the library's name (e.g., `/path/libdaos.so`). Used in
  conjunction with the `DAOS_UNS_LIBRARY` and `DAOS_INCLUDE_DIR` options.
  * `DAOS_UNS_LIBRARY` - This option controls the DAOS unified namespace library
  used by the VOL connector build process. It should be set to the full path to
  the DAOS `libduns` library, including the library's name
  (e.g., `/path/libduns.so`). Used in conjunction with the `DAOS_LIBRARY` and `DAOS_INCLUDE_DIR` options.
  * `DAOS_INCLUDE_DIR` - This option controls the DAOS include directory used by
  the VOL connector build process. Used in conjunction with the `DAOS_LIBRARY`
  and `DAOS_UNS_LIBRARY` options.
  * `MPI_C_COMPILER` - This option controls the MPI C compiler used by the VOL
  connector build process. It should be set to the full path to the MPI C
  compiler, including the name of the executable.

## 3. Testing and Usage
In the connector, each chunk is stored in a different DAOS dkey, and data in a single dkey is stored in a single DAOS storage target. Therefore, splitting the data into different chunks stripes the data across different dkeys and different storage targets. This improves I/O performance by allowing DAOS to read from or write to multiple storage targets at once.


The bandwidth improvement from using different storage targets is so vital that, if *H5Pset_chunk*() is not used, i.e., contiguous datasets, the connector will automatically set a chunk size. The connector, by default, tries to size these chunks to approximately 1 MiB. The environment variable **HDF5_DAOS_CHUNK_TARGET_SIZE** (in bytes) sets the chunk target size. Setting this variable to 0 disables automatic chunking, and contiguous datasets will stay contiguous (and will therefore only be stored on a single storage target). Better performance may be obtained by choosing a larger chunk target size, such as 4-8 MiB.

For further information on how to use the DAOS VOL connector with an HDF5 application,
as well as how to test that the VOL connector is functioning properly, please
refer to the DAOS VOL User's Guide under _docs/users_guide.pdf_.

## 4. More Information

### DAOS VOL
Design documentation for the DAOS VOL can be found under _docs/design_doc.pdf_.

Journal paper:
* J. Soumagne, J. Henderson, M. Chaarawi, N. Fortner, S. Breitenfeld, S. Lu, D. Robinson, E. Pourmal, J. Lombardi, "__Accelerating HDF5 I/O for Exascale Using DAOS__," in _IEEE Transactions on Parallel and Distributed Systems_, vol. 33, no. 4, pp. 903-914, April 2022. | [paper][doi_paper] |

### DAOS
DAOS installation and usage instructions can
be found on the DAOS website: https://docs.daos.io/

[vol-daos-release-svg]: https://img.shields.io/github/release/HDFGroup/vol-daos/all.svg
[vol-daos-release-link]: https://github.com/HDFGroup/vol-daos/releases
[doi_paper]: https://dx.doi.org/10.1109/TPDS.2021.3097884
