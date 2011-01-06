#
# Locate EPICS build-time tools which must be run on this host.
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

include(FindPackageMessage)

find_package(EPICSHostArch)

find_path(EPICS_HOST_BASE
  NAMES include/epicsVersion.h
  PATHS ${EPICS_BASE}
  NO_DEFAULT_PATH
  DOC "Location of an EPICS Base tools for the host system"
)

if(NOT EPICS_HOST_BASE AND EPICSHostTools_FIND_REQUIRED)
  message(FATAL_ERROR "Failed to find Host installation of EPICS Base")
endif(NOT EPICS_HOST_BASE AND EPICSHostTools_FIND_REQUIRED)

foreach(TOOL antelope e_flex
             dbToMenuH dbToRecordtypeH dbExpand
             makeBpt)
  string(TOUPPER ${TOOL} UTOOL)

  find_program(${UTOOL}
    NAMES ${TOOL}
    PATHS ${EPICS_HOST_BASE}
    PATH_SUFFIXES bin/${EPICS_HOST_ARCH}
                  bin
    NO_DEFAULT_PATH
    DOC "Location of ${TOOL} executable"
  )
  if(EPICSHostTools_FIND_REQUIRED AND NOT ${UTOOL})
    message(FATAL_ERROR "Missing ${TOOL}")
  endif(EPICSHostTools_FIND_REQUIRED AND NOT ${UTOOL})

endforeach(TOOL)

set(FLEX ${E_FLEX})

find_package_message(EPICSHostTools
   "Found EPICS Host Tools: ${EPICS_HOST_BASE}"
   "[${EPICS_HOST_BASE}]"
)
