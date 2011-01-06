
# RTEMS does not support shared libraries
SET_PROPERTY(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

# The Toolchain file must specify
# CMAKE_SYSTEM_PROCESSOR as on of
#   i.86
#   powerpc
# CMAKE_SYSTEM_VERSION with the RTEMS version of the tools
# RTEMS_BSP with a valid BSP name
# RTEMS_PREFIX location of tools
#
# eg
#
# set(CMAKE_SYSTEM_NAME RTEMS)
# set(CMAKE_SYSTEM_PROCESSOR powerpc)
# set(CMAKE_SYSTEM_VERSION 4.9)
# set(RTEMS_PREFIX "/usr")
# set(RTEMS_BSP mvme3100)
#

if(NOT CMAKE_SYSTEM_PROCESSOR)
  message(FATAL_ERROR "CMAKE_SYSTEM_PROCESSOR not set by toolchain")
endif(NOT CMAKE_SYSTEM_PROCESSOR)
if(NOT CMAKE_SYSTEM_VERSION)
  message(FATAL_ERROR "CMAKE_SYSTEM_VERSION not set by toolchain")
endif(NOT CMAKE_SYSTEM_VERSION)
if(NOT RTEMS_BSP)
  message(FATAL_ERROR "RTEMS_BSP not set by toolchain")
endif(NOT RTEMS_BSP)

set(RTEMS_PREFIX "/usr" CACHE PATH "Location of RTEMS toolchain")

# RTEMS is Unix-like (POSIX)
SET(UNIX 1)
SET(RTEMS 1)
SET(NEED_MUNCH ON)
if(CMAKE_SYSTEM_PROCESSOR MATCHES powerpc)
  SET(RTEMS_NEED_BSPEXT ON)
endif(CMAKE_SYSTEM_PROCESSOR MATCHES powerpc)

set(RTEMS_ARCH_PREFIX "/usr/${CMAKE_SYSTEM_PROCESSOR}-rtems${CMAKE_SYSTEM_VERSION}")

message(STATUS "Loading RTEMS toolchain from ${RTEMS_ARCH_PREFIX}")

SET(CMAKE_EXECUTABLE_SUFFIX ".elf")

list(APPEND CMAKE_SYSTEM_PREFIX_PATH
  "${RTEMS_ARCH_PREFIX}/${RTEMS_BSP}"
  "${RTEMS_ARCH_PREFIX}"
)

list(APPEND CMAKE_SYSTEM_INCLUDE_PATH
  "${RTEMS_ARCH_PREFIX}/${RTEMS_BSP}/lib/include"
  "${RTEMS_ARCH_PREFIX}/include"
)

list(APPEND CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES
  "${RTEMS_ARCH_PREFIX}/${RTEMS_BSP}/lib/include"
  "${RTEMS_ARCH_PREFIX}/include"
)
list(APPEND CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES
  "${RTEMS_ARCH_PREFIX}/${RTEMS_BSP}/lib/include"
  "${RTEMS_ARCH_PREFIX}/include"
)

list(APPEND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
  "${RTEMS_ARCH_PREFIX}/${RTEMS_BSP}/lib"
)

list(APPEND CMAKE_SYSTEM_LIBRARY_PATH
  "${RTEMS_ARCH_PREFIX}/${RTEMS_BSP}/lib"
)

#list(APPEND CMAKE_SYSTEM_PROGRAM_PATH

include(FindPkgConfig)

#set(RTEMS_LDPARTS "" CACHE INTERNAL "xx" FORCE)

pkg_check_modules(RTEMS REQUIRED ${CMAKE_SYSTEM_PROCESSOR}-rtems${CMAKE_SYSTEM_VERSION}-${RTEMS_BSP})

# uglyness to turn a ';' seperated list into a ' ' seperated list
set(_RTEMS_CFLAGS)
foreach(arg ${RTEMS_CFLAGS})
  if(arg MATCHES "^-g")
    # skip debug
  elseif(arg MATCHES "-O")
    # skip optimizations
  else(arg MATCHES "^-g")
    # use everything else
    set(_RTEMS_CFLAGS "${_RTEMS_CFLAGS} ${arg}")
  endif(arg MATCHES "^-g")
endforeach(arg ${RTEMS_CFLAGS})

set(CMAKE_C_FLAGS_INIT "${_RTEMS_CFLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${_RTEMS_CFLAGS}")

set(RTEMS_LDPARTS
  no-dpmem.rel no-mp.rel no-part.rel no-signal.rel
  no-rtmon.rel
)

set(_RTEMS_LDFLAGS "-u Init")
foreach(ldp ${RTEMS_LDPARTS})
  set(_RTEMS_LDFLAGS "${_RTEMS_LDFLAGS} ${RTEMS_ARCH_PREFIX}/${RTEMS_BSP}/lib/${ldp}")
endforeach(ldp ${RTEMS_LDPARTS})
set(_RTEMS_LDFLAGS "${_RTEMS_LDFLAGS} -static")

set(CMAKE_EXE_LINKER_FLAGS "${_RTEMS_LDFLAGS}")

message(STATUS "CFLAGS: ${CMAKE_C_FLAGS_INIT}")
message(STATUS "LDFLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
