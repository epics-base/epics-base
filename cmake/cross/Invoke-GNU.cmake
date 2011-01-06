#
# For GNU 
#

message(STATUS "-- Start cross build for GNU ${target}")

# Target OS
if(target MATCHES "mingw")
  set(SYSTEM_NAME "Windows")
  set(GNU_NAME "${target}")

elseif(target MATCHES "^[^-]*-rtems([0-9.]*)-[^-]*$")
  set(SYSTEM_NAME "RTEMS")
  string(REGEX REPLACE "^[^-]*-rtems([0-9.]*)-[^-]*$" "\\1" SYSTEM_VERSION ${target})
  string(REGEX REPLACE "^[^-]*-[^-]*-([^-]*)$" "\\1" RTEMS_BSP ${target})
  string(REGEX REPLACE "^([^-]*-[^-]*)-[^-]*$" "\\1" GNU_NAME ${target})
  message(STATUS "SYSTEM_VERSION=${SYSTEM_VERSION}")
  message(STATUS "RTEMS_BSP=${RTEMS_BSP}")
  message(STATUS "GNU_NAME=${GNU_NAME}")

else(target MATCHES "mingw")
  set(SYSTEM_NAME "Linux")
  set(GNU_NAME "${target}")

endif(target MATCHES "mingw")
# Target CPU
string(REGEX REPLACE "^([^-]*)-.*$" "\\1" SYSTEM_PROCESSOR ${target})

# which compilers to use for C and C++
find_program(C_COMPILER_${target}
  NAMES ${GNU_NAME}-gcc
  PATHS ${${GNU_NAME}_PREFIX}
        ${TOOL_PREFIX}
        /usr
  PATH_SUFFIXES bin
  DOC "${GNU_NAME} C Compiler"
)
set(C_COMPILER "${C_COMPILER_${target}}")

find_program(CXX_COMPILER_${target}
  NAMES ${GNU_NAME}-g++
  PATHS ${${GNU_NAME}_PREFIX}
        ${TOOL_PREFIX}
        /usr
  PATH_SUFFIXES bin
  DOC "${target} C++ Compiler"
)
set(CXX_COMPILER "${CXX_COMPILER_${target}}")

find_program(OBJCOPY_${target}
  NAMES ${GNU_NAME}-objcopy
  PATHS ${${GNU_NAME}_PREFIX}
        ${TOOL_PREFIX}
        /usr
  PATH_SUFFIXES bin
  DOC "${target} ELF utility"
)
set(OBJCOPY "${OBJCOPY_${target}}")

# where is the target environment located
#  Note: can add mode to list
find_path(TOOL_LOCATION_${target}
  NAMES include/stdio.h usr/include/stdio.h
  PATHS ${${GNU_NAME}_PREFIX}/${GNU_NAME}
        ${TOOL_PREFIX}/${GNU_NAME}
        /usr/${GNU_NAME}
  NO_DEFAULT_PATH
)
set(TOOL_LOCATION "${TOOL_LOCATION_${target}}")

if(NOT C_COMPILER)
  message(FATAL_ERROR "Failed to find C compiler ${GNU_NAME}-gcc")
endif(NOT C_COMPILER)
if(NOT CXX_COMPILER)
  message(FATAL_ERROR "Failed to find C++ compiler ${GNU_NAME}-g++")
endif(NOT CXX_COMPILER)
if(NOT OBJCOPY)
  message(FATAL_ERROR "Failed to find objcopy ${GNU_NAME}-objcopy")
endif(NOT OBJCOPY)
if(NOT TOOL_LOCATION)
  message(FATAL_ERROR "Failed to find toolchain prefix")
endif(NOT TOOL_LOCATION)

execute_process(
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/build-${target}
  RESULT_VARIABLE CMD1
)

set(TEMPLATE  "${EPICS_CMAKE_ROOT}/cross/Toolchain-GNU.cmake.in")
set(TOOLCHAIN "${CMAKE_BINARY_DIR}/build-${target}/Toolchain.cmake")

configure_file("${TEMPLATE}" "${TOOLCHAIN}" @ONLY)

execute_process(
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/build-${target}
  COMMAND ${CMAKE_COMMAND}
      -DEPICS_CMAKE_ROOT:PATH=${EPICS_CMAKE_ROOT}
      -DCMAKE_MODULE_PATH:PATH=${CMAKE_MODULE_PATH}
      -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${TOOLCHAIN}
      -DIMPORT_EXECUTABLES:FILEPATH=${CMAKE_BINARY_DIR}/hosttools.cmake
      -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
      ${CMAKE_SOURCE_DIR}
  RESULT_VARIABLE CMD2
)

if(CMD1 OR CMD2)
  message(FATAL_ERROR "Failed to find GNU toolchain")
endif(CMD1 OR CMD2)

# add targets if supported
if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")

  add_custom_target(${target} ALL
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/build-${target}
    COMMAND "$(MAKE)"
    VERBATIM
    DEPENDS ${HOSTTOOLTARGETS}
    COMMENT "Building for cross target GNU"
  )
endif(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
