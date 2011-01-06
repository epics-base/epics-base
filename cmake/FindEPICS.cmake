# Find EPICS
#
# Use as:
#  FIND_PACKAGE(EPICS REQUIRED)
#
# To use a local version first set:
#  SET(CMAKE_MODULE_PATH "/somedir/epics/cmake")
#
# Creates variables:
#  EPICS_BASE           - Location
#
#  EPICS_FOUND          - Where all components found?
#  EPICS_INCLUDE_DIRS   - CPP search path (global and OS)
#
#  EPICS_Com_LIBRARIES  - libCom and dependencies
#  EPICS_*_LIBRARY      - Individual libs w/o dependencies
#  EPICS_HOST_LIBRARIES - All *Host libraries and cas/gdd
#  EPICS_IOC_LIBRARIES  - All *Ioc libraries
#
#  EPICS_INCLUDE_DIR
#  EPICS_OS_INCLUDE_DIR
#

# EPICS OS classes
#     AIX cygwin32 Darwin freebsd
#     hpux interix Linux osf RTEMS
#     solaris vxWorks WIN32

set(_EPICS_ALL_LIBS
  cas gdd asHost dbStaticHost registryIoc
  recIoc softDevIoc miscIoc rsrvIoc dbtoolsIoc asIoc
  dbIoc registryIoc dbStaticIoc ca Com
)

find_package(EPICSArch)

include(FindPackageHandleStandardArgs)

find_path(EPICS_BASE
  NAMES include/epicsVersion.h
        dbd/base.dbd
  PATHS /usr/lib/epics
        /opt/epics/current
  DOC "Location of EPICS Base"
)

if(NOT EPICS_BASE)
  if(EPICS_FIND_REQUIRED)
    message(FATAL_ERROR "Failed to find EPICS Base location")
  endif(EPICS_FIND_REQUIRED)
endif(NOT EPICS_BASE)

find_path(EPICS_INCLUDE_DIR epicsVersion.h
  PATHS ${EPICS_BASE}/include
  DOC "EPICS include dir"
  NO_DEFAULT_PATH
)

if(EPICS_INCLUDE_DIR)

  find_path(EPICS_OS_INCLUDE_DIR
    NAMES epicsMath.h osdTime.h osiUnistd.h
    PATHS ${EPICS_INCLUDE_DIR}/os/
    PATH_SUFFIXES ${OS_CLASS}
    DOC "EPICS OSD include dir"
    NO_DEFAULT_PATH
  )

  if(EPICS_OS_INCLUDE_DIR)
    SET(EPICS_INCLUDE_DIRS
      ${EPICS_INCLUDE_DIR}
      ${EPICS_OS_INCLUDE_DIR}
    )

  endif(EPICS_OS_INCLUDE_DIR)

endif(EPICS_INCLUDE_DIR)

if(EPICS_INCLUDE_DIR AND EPICS_OS_INCLUDE_DIR)

  foreach(COMP ${_EPICS_ALL_LIBS})
    find_library(EPICS_${COMP}_LIBRARY ${COMP}
      PATHS ${EPICS_BASE}/lib
      PATH_SUFFIXES ${T_A}
      NO_DEFAULT_PATH
      DOC "Location of the EPICS ${COMP} library"
    )

  endforeach(COMP)

endif(EPICS_INCLUDE_DIR AND EPICS_OS_INCLUDE_DIR)

# Ensure all components are present if needed
# When find_package() is invoked with REQUIRED
# ensure that all of the following are set
find_package_handle_standard_args(EPICS DEFAULT_MSG
  EPICS_BASE
  EPICS_INCLUDE_DIR EPICS_OS_INCLUDE_DIR
  EPICS_Com_LIBRARY
  EPICS_ca_LIBRARY
  EPICS_cas_LIBRARY
  EPICS_gdd_LIBRARY
  EPICS_asHost_LIBRARY
  EPICS_dbStaticHost_LIBRARY
  EPICS_registryIoc_LIBRARY
  EPICS_recIoc_LIBRARY
  EPICS_softDevIoc_LIBRARY
  EPICS_miscIoc_LIBRARY
  EPICS_rsrvIoc_LIBRARY
  EPICS_dbtoolsIoc_LIBRARY
  EPICS_asIoc_LIBRARY
  EPICS_dbIoc_LIBRARY
  EPICS_registryIoc_LIBRARY
  EPICS_dbStaticIoc_LIBRARY
)

set(EPICS_Com_LIBRARIES ${EPICS_Com_LIBRARY})

#TODO: error checking below

if(NOT RTEMS)
  find_package(Threads REQUIRED)
  list(APPEND EPICS_Com_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
endif(NOT RTEMS)

if(UNIX AND NOT RTEMS)
  find_library(LIBRT rt)
  find_library(LIBDL dl)
  list(APPEND EPICS_Com_LIBRARIES ${LIBRT} ${LIBDL})
endif(UNIX AND NOT RTEMS)

if(RTEMS)
  find_library(RTEMSCPU rtemscpu)
  find_library(RTEMSNFS nfs)
  list(APPEND EPICS_Com_LIBRARIES ${RTEMSCPU} ${RTEMSNFS})
  if(RTEMS_NEED_BSPEXT)
    find_library(BSPEXT bspExt)
    list(APPEND EPICS_Com_LIBRARIES ${BSPEXT})
  endif(RTEMS_NEED_BSPEXT)
endif(RTEMS)

# Libraries needed to build an IOC
set(EPICS_BASE_IOC_LIBS
  ${EPICS_recIoc_LIBRARY}
  ${EPICS_softDevIoc_LIBRARY}
  ${EPICS_miscIoc_LIBRARY}
  ${EPICS_rsrvIoc_LIBRARY}
  ${EPICS_dbtoolsIoc_LIBRARY}
  ${EPICS_asIoc_LIBRARY}
  ${EPICS_dbIoc_LIBRARY}
  ${EPICS_registryIoc_LIBRARY}
  ${EPICS_dbStaticIoc_LIBRARY}
  ${EPICS_ca_LIBRARY}
  ${EPICS_Com_LIBRARIES}
)

set(EPICS_INSTALL_BIN "bin/${T_A}" CACHE FILEPATH "Location for installed binaries")
set(EPICS_INSTALL_LIB "lib/${T_A}" CACHE FILEPATH "Location for all libraries")
set(EPICS_INSTALL_INCLUDE "include" CACHE FILEPATH "Location for C/C++ headers")
set(EPICS_INSTALL_INCLUDE_OS "include/os/${OS_CLASS}" CACHE FILEPATH "Location for OSD C/C++ Headers")
set(EPICS_INSTALL_DBD "dbd" CACHE FILEPATH "Location for EPICS DBD files")
set(EPICS_INSTALL_DB "db" CACHE FILEPATH "Location for EPICS DB files")

mark_as_advanced(
  EPICS_BIN_DIR EPICS_LIB_DIR
  EPICS_INCLUDE_DIR EPICS_OS_INCLUDE_DIR
  EPICS_DBD_DIR EPICS_DB_DIR
  EPICS_CMAKE_DIR
)
