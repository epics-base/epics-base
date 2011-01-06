#
# This is the general dispatcher which decides what to do with
# cross build targets
#

set(TARGETS ""
  CACHE STRING "List of cross targets"
)

foreach(target ${TARGETS})

  if(target MATCHES "linux")
    include(cross/Invoke-GNU)

  elseif(target MATCHES "mingw")
    include(cross/Invoke-GNU)

  elseif(target MATCHES "rtems")
    include(cross/Invoke-GNU)

  else(target MATCHES "linux")
    message(STATUS "Unknown target ${target}")

  endif(target MATCHES "linux")

endforeach(target)
