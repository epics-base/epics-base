#
# This is the general dispatcher which decides what to do with
# cross build targets
#

set(TARGETS ""
  CACHE STRING "List of cross targets"
)

foreach(target ${TARGETS})

  if(target MATCHES "linux")
    include(${CMAKE_SOURCE_DIR}/cmake/cross/Invoke-GNU.cmake)

  elseif(target MATCHES "mingw")
    include(${CMAKE_SOURCE_DIR}/cmake/cross/Invoke-GNU.cmake)

  elseif(target MATCHES "rtems")
    include(${CMAKE_SOURCE_DIR}/cmake/cross/Invoke-GNU.cmake)

  endif(target MATCHES "linux")

endforeach(target)
