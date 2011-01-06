
include_directories(
  ${LIBCOM_DIRS}
)

set(LIB_HEADERS
  epicsRtemsInitHooks.h
)

set(LIB_SRCS
  rtems_init.c
  rtems_config.c
  rtems_netconfig.c
  rtems_util.c
  setBootConfigFromNVRAM.c
  epicsRtemsInitHookPre.c
  epicsRtemsInitHookPost.c
)

set(LIB_DEPS )

if(RTEMS)
  set(EXTRA_DEPS
    ${RTEMSCPU}
    ${RTEMSNFS}
  )
  if(RTEMS_NEED_BSPEXT)
    set(EXTRA_DEPS
      ${EXTRA_DEPS}
      ${BSPEXT}
    )
  endif(RTEMS_NEED_BSPEXT)
else(RTEMS)
  set(EXTRA_DEPS )
endif(RTEMS)

add_both_library(rtemsCom
  SONUM ${BASE_SOVERSION}
  SOURCES ${LIB_SRCS}
  LINK_LIBRARIES ${LIB_DEPS}
  EXTRA_LIB ${EXTRA_DEPS}
)

install(FILES ${LIB_HEADERS}
  DESTINATION ${EPICS_INSTALL_INCLUDE}
)
