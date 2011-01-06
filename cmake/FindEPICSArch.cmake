# Determine EPICS Arch and OS Class
#
# Use as:
#  find_package(EPICSArch)
#
# Creates variables:
#  T_A        - Target Archetecture
#  OS_CLASS   - Primary (first) OSI system class
#  OS_CLASSES - List of all OSI classes
#  IOC        - True if target can build IOC things
#  HOST       - True if target can build Host things
#

if(UNIX)
  if(CMAKE_SYSTEM_NAME MATCHES Linux)
    set(HOST 1)
    set(IOC 1)
    set(OS_CLASS Linux)
    set(OS_CLASSES Linux posix default)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^i.86$")
      set(T_A "linux-x86")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^powerpc")
      set(T_A "linux-ppc")
    else(CMAKE_SYSTEM_PROCESSOR MATCHES "^i.86$")
      message(FATAL_ERROR "Unknown Linux Variant: ${CMAKE_SYSTEM_PROCESSOR}")
    endif(CMAKE_SYSTEM_PROCESSOR MATCHES "^i.86$")

  elseif(RTEMS)
    set(HOST 0)
    set(IOC 1)
    set(OS_CLASS RTEMS)
    set(OS_CLASSES RTEMS posix default)
    set(T_A "RTEMS-${RTEMS_BSP}")

  else(CMAKE_SYSTEM_NAME MATCHES Linux)
    message(FATAL_ERROR "Unknown *nix Variant: ${CMAKE_SYSTEM_NAME}")
  endif(CMAKE_SYSTEM_NAME MATCHES Linux)

elseif(WIN32)
  set(HOST 1)
  set(IOC 1)
  set(OS_CLASS WIN32)
  set(OS_CLASSES WIN32 default)
  if(MINGW)
    set(T_A "win32-x86-mingw")
  elseif(CYGWIN)
    set(T_A "win32-x86-cygwin")
  elseif(MSVC)
    set(T_A "win32-x86")
  elseif(BORLAND)
    set(T_A "win32-x86-borland")
  else(MINGW)
    message(FATAL_ERROR "Unknown Windows variant")
  endif(MINGW)

else(UNIX)
  message(FATAL_ERROR "Unable to determine EPICS OS class")
endif(UNIX)

message(STATUS "Target: ${T_A}")
message(STATUS "Target Class: ${OS_CLASS}")
