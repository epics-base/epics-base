#
# Locate EPICS build-time tools which can be run on this host.
#
# Set EPICS_HOST_SEARCH to a list of candidates for
# EPICS_HOST_BASE.
# and set EPICS_HOST_ARCH as appropriate
#
# Created:
#   EPICS_HOST_BASE
#   ANTELOPE
#   FLEX
#   FLEXSKEL
#   DBTOMENUH
#   DBTORECORDTYPEH
#   DBEXPAND
#

find_package(EPICSHostArch)

find_path(EPICS_HOST_BASE
  NAMES include/epicsVersion.h
  PATHS /usr/epics/base
        /usr/local/epics/base
  DOC "Location of an EPICS Base installation for the host system"
)

if(NOT EPICS_HOST_BASE AND EPICSHostTools_FIND_REQUIRED)
  message(FATAL_ERROR "Failed to find Host installation of EPICS Base")
endif(NOT EPICS_HOST_BASE AND EPICSHostTools_FIND_REQUIRED)

find_program(ANTELOPE
  NAMES antelope
  PATHS ${EPICS_HOST_BASE}
  PATH_SUFFIXES bin/${EPICS_HOST_ARCH}
                bin
  NO_DEFAULT_PATH
  DOC "Location of antelope executable"
)
if(EPICSHostTools_FIND_REQUIRED AND NOT ANTELOPE)
  message(FATAL_ERROR "Missing antelope")
endif(EPICSHostTools_FIND_REQUIRED AND NOT ANTELOPE)

find_program(FLEX
  NAMES e_flex
  PATHS ${EPICS_HOST_BASE}
  PATH_SUFFIXES bin/${EPICS_HOST_ARCH}
                bin
  NO_DEFAULT_PATH
  DOC "Location of e_flex executable"
)
if(EPICSHostTools_FIND_REQUIRED AND NOT FLEX)
  message(FATAL_ERROR "Missing e_flex")
endif(EPICSHostTools_FIND_REQUIRED AND NOT FLEX)

find_program(DBTOMENUH
  NAMES dbToMenuH
  PATHS ${EPICS_HOST_BASE}
  PATH_SUFFIXES bin/${EPICS_HOST_ARCH}
                bin
  NO_DEFAULT_PATH
  DOC "Location of dbToMenuH executable"
)
if(EPICSHostTools_FIND_REQUIRED AND NOT DBTOMENUH)
  message(FATAL_ERROR "Missing dbToMenuH")
endif(EPICSHostTools_FIND_REQUIRED AND NOT DBTOMENUH)

find_program(DBTORECORDTYPEH
  NAMES dbToRecordtypeH
  PATHS ${EPICS_HOST_BASE}
  PATH_SUFFIXES bin/${EPICS_HOST_ARCH}
                bin
  NO_DEFAULT_PATH
  DOC "Location of dbToRecordtypeH executable"
)
if(EPICSHostTools_FIND_REQUIRED AND NOT DBTORECORDTYPEH)
  message(FATAL_ERROR "Missing dbToRecordtypeH")
endif(EPICSHostTools_FIND_REQUIRED AND NOT DBTORECORDTYPEH)

find_program(DBEXPAND
  NAMES dbExpand
  PATHS ${EPICS_HOST_BASE}
  PATH_SUFFIXES bin/${EPICS_HOST_ARCH}
                bin
  NO_DEFAULT_PATH
  DOC "Location of dbExpand executable"
)
if(EPICSHostTools_FIND_REQUIRED AND NOT DBEXPAND)
  message(FATAL_ERROR "Missing dbExpand")
endif(EPICSHostTools_FIND_REQUIRED AND NOT DBEXPAND)

find_program(MAKEBPT
  NAMES makeBpt
  PATHS ${EPICS_HOST_BASE}
  PATH_SUFFIXES bin/${EPICS_HOST_ARCH}
                bin
  NO_DEFAULT_PATH
  DOC "Location of makeBpt executable"
)
if(EPICSHostTools_FIND_REQUIRED AND NOT MAKEBPT)
  message(FATAL_ERROR "Missing makeBpt")
endif(EPICSHostTools_FIND_REQUIRED AND NOT MAKEBPT)

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

if(NOT EPICS_FIND_QUIETLY)
  message(STATUS "Found EPICS Host Tools: ${EPICS_HOST_BASE}")
endif(NOT EPICS_FIND_QUIETLY)
