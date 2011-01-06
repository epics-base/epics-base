# Find EPICS
#
# Use as:
#  FIND_PACKAGE(EPICS REQUIRED Com ca ...)
#
# To use a local version first set:
#  SET(CMAKE_MODULE_PATH "/somedir/epics/cmake")
#
# Creates variables
#  EPICS_FOUND
#  EPICS_INCLUDE_DIRS
#  EPICS_LIBRARIES
#  EPICS_${COMP}_LIBRARY # where COMP is Com, ca, ...
#  EPICS_${COMP}_FOUND
#

# EPICS OS classes
#     AIX cygwin32 Darwin freebsd
#     hpux interix Linux osf RTEMS
#     solaris vxWorks WIN32

find_package(EPICSArch)

set(EPICS_BASE EPICS_BASE-NOTFOUND
  CACHE FILEPATH
  "Location of EPICS Base install"
)

find_package(EPICSHostTools REQUIRED)

FIND_PATH(EPICS_INCLUDE_DIR epicsVersion.h
  PATHS ${EPICS_BASE}/include
        $ENV{EPICS_BASE}/include
        /usr/epics/base
        /usr/local/epics/base
  DOC "EPICS include dir"
)

IF(NOT EPICS_INCLUDE_DIR)
  IF(EPICS_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Failed to find EPICS")
  ENDIF(EPICS_FIND_REQUIRED)
ELSE(NOT EPICS_INCLUDE_DIR)

  IF(NOT EPICS_FIND_QUIETLY)
    MESSAGE(STATUS "Found EPICS: ${EPICS_INCLUDE_DIR}")
  ENDIF(NOT EPICS_FIND_QUIETLY)

  FIND_PATH(EPICS_OS_INCLUDE_DIR
    NAMES epicsMath.h osdTime.h osiUnistd.h
    PATHS ${EPICS_INCLUDE_DIR}/os/
    PATH_SUFFIXES ${OS_CLASS}
    DOC "EPICS OSD include dir"
  )


  IF(EPICS_OS_INCLUDE_DIR)
    SET(EPICS_INCLUDE_DIRS
      ${EPICS_INCLUDE_DIR}
      ${EPICS_OS_INCLUDE_DIR}
    )
    SET(EPICS_FOUND 1)

  ELSEIF(EPICS_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Failed to find EPICS OS directory")
  ENDIF(EPICS_OS_INCLUDE_DIR)

ENDIF(NOT EPICS_INCLUDE_DIR)

IF(EPICS_FOUND)

  FOREACH(COMP ${EPICS_FIND_COMPONENTS})
    FIND_LIBRARY(EPICS_${COMP}_LIBRARY ${COMP}
      PATHS $ENV{EPICS_DIR}/lib
            $ENV{EPICS_BASE}/lib
            $ENV{EPICS}/base/lib
      PATH_SUFFIXES ${T_A}
      DOC "Location of the EPICS ${COMP} library"
    )

    IF(EPICS_${COMP}_LIBRARY)
      SET(EPICS_${COMP}_FOUND 1)
      SET(EPICS_LIBRARIES ${EPICS_LIBRARIES} ${EPICS_${COMP}_LIBRARY})
      IF(NOT EPICS_FIND_QUIETLY)
        MESSAGE(STATUS "Found EPICS ${COMP}: ${EPICS_${COMP}_LIBRARY}")
      ENDIF(NOT EPICS_FIND_QUIETLY)
    ELSEIF(EPICS_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Failed to find required EPICS library ${COMP}")
    ELSEIF(NOT EPICS_FIND_QUIETLY)
      MESSAGE(STATUS "Failed to find required EPICS library ${COMP}")
    ENDIF(EPICS_${COMP}_LIBRARY)
  ENDFOREACH(COMP)

ENDIF(EPICS_FOUND)
