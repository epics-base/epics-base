
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${LIBCOM_DIRS}
  ${CMAKE_SOURCE_DIR}/src/ca
  ${CMAKE_SOURCE_DIR}/src/dbStatic
  ${CMAKE_SOURCE_DIR}/src/db
  ${CMAKE_BINARY_DIR}/src/db
)

# antelope

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c
  COMMAND ${ANTELOPE} ${CMAKE_CURRENT_SOURCE_DIR}/asLib.y
  DEPENDS asLib.y
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/asLib.c
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c ${CMAKE_CURRENT_BINARY_DIR}/asLib.c
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c
)

# lex

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
  COMMAND ${FLEX} -8 -I -S${FLEXSKEL} ${CMAKE_CURRENT_SOURCE_DIR}/asLib_lex.l
  DEPENDS asLib_lex.l
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/asLib_lex.c
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c ${CMAKE_CURRENT_BINARY_DIR}/asLib_lex.c
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
)

set_source_files_properties(asLib.c
  PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/asLib_lex.c
)


set(LIB_HEADERS
  asLib.h
  asDbLib.h
  asCa.h
  asTrapWrite.h
  asIocRegister.h
)

set(HOST_SRC
  asLib.c
  asTrapWrite.c
)

set(IOC_SRC
  ${HOST_SRC}
  asDbLib.c
  asCa.c
  asIocRegister.c
)

set(HOST_DEPS dbStaticHost ca Com)
set(IOC_DEPS dbIoc dbStaticIoc ca Com)

add_both_library(asHost
  SONUM ${BASE_SOVERSION}
  SOURCES ${HOST_SRC}
  LINK_LIBRARIES ${HOST_DEPS}
)
add_both_library(asIoc
  SONUM ${BASE_SOVERSION}
  SOURCES ${IOC_SRC}
  LINK_LIBRARIES ${IOC_DEPS}
)

#asHost_RCS = asHost.rc
#asIoc_RCS = asIoc.rc

if(HOST)
  add_executable(ascheck
    ascheck.c
  )
  set_target_properties(ascheck
    PROPERTIES HAS_CXX YES
  )
  target_link_libraries(ascheck
    asHost${UTIL} dbStaticHost${UTIL} ca${UTIL} Com${UTIL}
  )

  install(TARGETS ascheck
    RUNTIME DESTINATION ${EPICS_INSTALL_BIN}
  )

endif(HOST)

install(FILES ${LIB_HEADERS}
  DESTINATION ${EPICS_INSTALL_INCLUDE}
)
