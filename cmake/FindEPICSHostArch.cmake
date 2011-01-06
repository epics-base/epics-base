# Determine EPICS Host Arch and OS Class
#
# Use as:
#  find_package(EPICSHostArch)
#
# Creates variables:
#  EPICS_HOST_ARCH - Target Archetecture
#  HOST_OS_CLASS   - Primary (first) OSI system class
#  HOST_OS_CLASSES - List of all OSI classes
#  HOST_IOC        - True if host can build (native) IOC things
#

if(CMAKE_HOST_UNIX)
  if(CMAKE_HOST_SYSTEM_NAME MATCHES Linux)
    set(HOST_IOC 1)
    set(HOST_OS_CLASS Linux)
    set(HOST_OS_CLASSES Linux posix default)
    if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^i.86$")
      set(EPICS_HOST_ARCH "linux-x86")
    elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^powerpc")
      set(EPICS_HOST_ARCH "linux-ppc")
    else(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^i.86$")
      message(FATAL_ERROR "Unknown Linux Variant: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
    endif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^i.86$")

  else(CMAKE_HOST_SYSTEM_NAME MATCHES Linux)
    message(FATAL_ERROR "Unknown *nix Variant: ${CMAKE_HOST_SYSTEM_NAME}")
  endif(CMAKE_HOST_SYSTEM_NAME MATCHES Linux)

elseif(CMAKE_HOST_WIN32)
  set(HOST_IOC 1)
  set(HOST_OS_CLASS WIN32)
  set(HOST_OS_CLASSES WIN32 default)
  
  # Don't support cross build on windows
  if(MINGW)
    set(EPICS_HOST_ARCH "win32-x86-mingw")
  elseif(CYGWIN)
    set(EPICS_HOST_ARCH "win32-x86-cygwin")
  elseif(MSVC)
    set(EPICS_HOST_ARCH "win32-x86")
  elseif(BORLAND)
    set(EPICS_HOST_ARCH "win32-x86-borland")
  else(MINGW)
    message(FATAL_ERROR "Unknown Windows variant: ${CMAKE_SYSTEM}")
  endif(MINGW)

else(CMAKE_HOST_UNIX)
  message(FATAL_ERROR "Unable to determine EPICS host OS class")
endif(CMAKE_HOST_UNIX)

message(STATUS "Host: ${EPICS_HOST_ARCH}")
message(STATUS "Host Class: ${HOST_OS_CLASS}")
