cmake_minimum_required(VERSION 2.6)

project(EPICSBASE C CXX)

enable_testing()

# Local modules
set(EPICS_CMAKE_ROOT "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${EPICS_CMAKE_ROOT}")

find_package(EPICSHostArch)
find_package(EPICSArch)

if(HOST_OS_CLASS STREQUAL Linux)
  if(MINGW OR RTEMS)
    # Cross building via. mingw doesn't work with shared libraries
    set(DEFAULT_SHARED_LIBS OFF CACHE BOOL "xx")
  endif(MINGW OR RTEMS)
endif(HOST_OS_CLASS STREQUAL Linux)
set(DEFAULT_SHARED_LIBS ON CACHE BOOL "xx")
mark_as_advanced(DEFAULT_SHARED_LIBS)

option(BUILD_SHARED_LIBS "Build Shared Libraries" ${DEFAULT_SHARED_LIBS})
option(UTILSTATIC "Utilities use static linkage" ON)
if(UTILSTATIC)
  set(UTIL Static)
else(UTILSTATIC)
  set(UTIL Shared)
endif(UTILSTATIC)

include(BothLibraries)
include(EPICSDBD)
include(BuildIOC)

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Build type not set.  Default to Debug")
  set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)

include(TestCXXAcceptsFlag)
macro(ADD_CXX_FLAG FLAG VAR)
  check_cxx_accepts_flag( "${FLAG}" ${VAR} )
  if(${VAR})
    add_definitions( "${FLAG}" )
  endif(${VAR})
endmacro(ADD_CXX_FLAG)

if(${CMAKE_BUILD_TYPE} MATCHES Debug)
  add_cxx_flag( -Wall USE_WALL)
#  add_cxx_flag( -Weffc++ USE_EFFCXX)
#  add_cxx_flag( -Woverloaded-virtual USE_OVERVIRT)
#  add_cxx_flag( -Werror USE_WERR)
endif(${CMAKE_BUILD_TYPE} MATCHES Debug)

find_package(Perl REQUIRED)

if(NOT RTEMS)
  find_package(Threads REQUIRED)
endif(NOT RTEMS)

if(UNIX AND NOT RTEMS)
  find_library(LIBRT rt)
  find_library(LIBDL dl)
endif(UNIX AND NOT RTEMS)

if(RTEMS)
  find_library(RTEMSCPU rtemscpu)
  find_library(RTEMSNFS nfs)
  if(RTEMS_NEED_BSPEXT)
    find_library(BSPEXT bspExt)
  endif(RTEMS_NEED_BSPEXT)
endif(RTEMS)

set(EPICS_CONFIG ${CMAKE_SOURCE_DIR}/configure)

set(EPICS_OSI_DIRS
  ${CMAKE_SOURCE_DIR}/src/libCom/osi
)
foreach(CLASS ${OS_CLASSES})
  set(EPICS_OSI_DIRS
    ${EPICS_OSI_DIRS}
    ${CMAKE_SOURCE_DIR}/src/libCom/osi/os/${CLASS}
  )
endforeach(CLASS)

set(LIBCOM_DIRS
  ${EPICS_OSI_DIRS}
  ${CMAKE_BINARY_DIR}/src/libCom
  ${CMAKE_SOURCE_DIR}/src/libCom/bucketLib
  ${CMAKE_SOURCE_DIR}/src/libCom/calc
  ${CMAKE_SOURCE_DIR}/src/libCom/cppStd
  ${CMAKE_SOURCE_DIR}/src/libCom/cvtFast
  ${CMAKE_SOURCE_DIR}/src/libCom/cxxTemplates
  ${CMAKE_SOURCE_DIR}/src/libCom/dbmf
  ${CMAKE_SOURCE_DIR}/src/libCom/ellLib
  ${CMAKE_SOURCE_DIR}/src/libCom/env
  ${CMAKE_SOURCE_DIR}/src/libCom/error
  ${CMAKE_SOURCE_DIR}/src/libCom/fdmgr
  ${CMAKE_SOURCE_DIR}/src/libCom/freeList
  ${CMAKE_SOURCE_DIR}/src/libCom/gpHash
  ${CMAKE_SOURCE_DIR}/src/libCom/iocsh
  ${CMAKE_SOURCE_DIR}/src/libCom/logClient
  ${CMAKE_SOURCE_DIR}/src/libCom/macLib
  ${CMAKE_SOURCE_DIR}/src/libCom/misc
  ${CMAKE_SOURCE_DIR}/src/libCom/ring
  ${CMAKE_SOURCE_DIR}/src/libCom/taskwd
  ${CMAKE_SOURCE_DIR}/src/libCom/test
  ${CMAKE_SOURCE_DIR}/src/libCom/timer
  ${CMAKE_SOURCE_DIR}/src/libCom/tsDefs
)

set(IOC_LIBRARIES
  Com${UTIL}
  ca${UTIL}
  miscIoc${UTIL}
  recIoc${UTIL}
  softDevIoc${UTIL}
  rsrvIoc${UTIL}
  #dbtools${UTIL}
  asIoc${UTIL}
  dbIoc${UTIL}
  registryIoc${UTIL}
  dbStaticIoc${UTIL}
)
if(RTEMS)
  list(APPEND IOC_LIBRARIES rtemsCom${UTIL})
endif(RTEMS)

# Base Version

set(EPICS_VERSION 3)
set(EPICS_REVISION 14)
set(EPICS_MODIFICATION 11)
set(EPICS_PATCH_LEVEL 0)
set(EPICS_CVS_SNAPSHOT "")
set(EPICS_VERSION_STRING "${EPICS_VERSION}.${EPICS_REVISION}.${EPICS_MODIFICATION}")
set(EPICS_SITE_VERSION "")

set(BASE_SOVERSION ${EPICS_VERSION}.${EPICS_REVISION}.${EPICS_MODIFICATION})

# Define install layout

set(EPICS_BIN_DIR "bin/${T_A}" CACHE FILEPATH "Location for installed binaries")
set(EPICS_LIB_DIR "lib/${T_A}" CACHE FILEPATH "Location for all libraries")
set(EPICS_INCLUDE_DIR "include" CACHE FILEPATH "Location for C/C++ headers")
set(EPICS_OS_INCLUDE_DIR "include/os/${OS_CLASS}" CACHE FILEPATH "Location for OSD C/C++ Headers")
set(EPICS_DBD_DIR "dbd" CACHE FILEPATH "Location for EPICS DBD files")
set(EPICS_DB_DIR "db" CACHE FILEPATH "Location for EPICS DB files")
set(EPICS_CMAKE_DIR "cmake" CACHE FILEPATH "Location for CMake files which allow Apps to find EPICS Base")

mark_as_advanced(
  EPICS_BIN_DIR EPICS_LIB_DIR
  EPICS_INCLUDE_DIR EPICS_OS_INCLUDE_DIR
  EPICS_DBD_DIR EPICS_DB_DIR
  EPICS_CMAKE_DIR
)

if(CMAKE_CROSSCOMPILING AND NOT IMPORT_EXECUTABLES)
  find_package(EPICSHostTools REQUIRED)

  # locate internal code generators
  find_program(AITGEN
    NAMES aitGen
    PATHS ${EPICS_HOST_BASE}
    PATH_SUFFIXES bin/${EPICS_HOST_ARCH}
                  bin
    NO_DEFAULT_PATH
    DOC "Location of aitGen executable"
  )
  if(EPICSHostTools_FIND_REQUIRED AND NOT AITGEN)
    message(FATAL_ERROR "Missing aitGen")
  endif(EPICSHostTools_FIND_REQUIRED AND NOT AITGEN)

  find_program(GENAPPS
    NAMES genApps
    PATHS ${EPICS_HOST_BASE}
    PATH_SUFFIXES bin/${EPICS_HOST_ARCH}
                  bin
    NO_DEFAULT_PATH
    DOC "Location of genApps executable"
  )
  if(EPICSHostTools_FIND_REQUIRED AND NOT GENAPPS)
    message(FATAL_ERROR "Missing genApps")
  endif(EPICSHostTools_FIND_REQUIRED AND NOT GENAPPS)

else(CMAKE_CROSSCOMPILING AND NOT IMPORT_EXECUTABLES)

  if(IMPORT_EXECUTABLES)
    include(${IMPORT_EXECUTABLES})
    set(IMPPREF "host_")
  else(IMPORT_EXECUTABLES)
    set(IMPPREF "")
  endif(IMPORT_EXECUTABLES)

  set(ANTELOPE ${IMPPREF}antelope)
  set(FLEX ${IMPPREF}e_flex)
  set(DBTOMENUH ${IMPPREF}dbToMenuH)
  set(DBTORECORDTYPEH ${IMPPREF}dbToRecordtypeH)
  set(DBEXPAND ${IMPPREF}dbExpand)
  set(MAKEBPT ${IMPPREF}makeBpt)
  set(GENAPPS ${IMPPREF}genApps)
  set(AITGEN ${IMPPREF}aitGen)

endif(CMAKE_CROSSCOMPILING AND NOT IMPORT_EXECUTABLES)

set(FLEXSKEL ${CMAKE_SOURCE_DIR}/src/toolsComm/flex/flex.skel.static)

install(FILES 
    cmake/FindEPICS.cmake
    cmake/FindEPICSArch.cmake
    cmake/FindEPICSHostArch.cmake
    cmake/FindEPICSHostTools.cmake
    cmake/EPICSDBD.cmake
    cmake/BuildIOC.cmake
  DESTINATION ${EPICS_CMAKE_DIR}
)

add_definitions(
  -D_POSIX_C_SOURCE=199506L
  -D_POSIX_THREADS
  -D_XOPEN_SOURCE=500
  -D_BSD_SOURCE
  -D_REENTRANT
)

add_subdirectory(src/libCom)

if(HOST)
  add_subdirectory(src/toolsComm/antelope)

  add_subdirectory(src/toolsComm/flex)
endif(HOST)

add_subdirectory(src/ca)
add_subdirectory(src/dbStatic)

add_subdirectory(src/registry)
add_subdirectory(src/bpt)
add_subdirectory(src/db)
add_subdirectory(src/as)

if(HOST)
  add_subdirectory(src/util)
endif(HOST)

add_subdirectory(src/dbtools)

if(HOST)
  add_subdirectory(src/catools)
endif(HOST)

add_subdirectory(src/rsrv)
add_subdirectory(src/rec)
add_subdirectory(src/misc)
add_subdirectory(src/dev/softDev)
add_subdirectory(src/dev/testDev)

#TODO vxWorks

if(RTEMS)
  add_subdirectory(src/RTEMS/base)
endif(RTEMS)

if(HOST OR RTEMS) #TODO on vxWorks
  add_subdirectory(src/libCom/test)
#TODO db/test
endif(HOST OR RTEMS)

add_subdirectory(src/softIoc)
add_subdirectory(src/gdd)
add_subdirectory(src/cas)

if(HOST AND BUILD_SHARED_LIBS)
  add_subdirectory(src/cap5)
endif(HOST AND BUILD_SHARED_LIBS)


if(NOT CMAKE_CROSSCOMPILING)
  set(HOSTTOOLTARGETS
    antelope e_flex dbToMenuH dbToRecordtypeH dbExpand makeBpt genApps aitGen
  )
  export(
    TARGETS ${HOSTTOOLTARGETS}
    NAMESPACE host_
    FILE ${CMAKE_BINARY_DIR}/hosttools.cmake
  )

  include(${CMAKE_SOURCE_DIR}/cmake/cross/Control.cmake)
endif(NOT CMAKE_CROSSCOMPILING)

# Install things related to Makefile based build system

file(GLOB conf_files
  configure/CONFIG*
  configure/RELEASE
  configure/RULES*
  configure/Sample.Makefile
)
file(GLOB confos_files
  configure/os/*
)

install(FILES ${conf_files}
  DESTINATION configure
)

install(FILES ${confos_files}
  DESTINATION configure/os
)

if(HOST)
  file(GLOB makeperls
    src/tools/*.pl
  )

  install(FILES ${makeperls}
    DESTINATION bin/${T_A}
  )
endif(HOST)
