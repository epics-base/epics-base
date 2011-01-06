
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${LIBCOM_DIRS}
  ${CMAKE_SOURCE_DIR}/src/db
  ${CMAKE_BINARY_DIR}/src/db
  ${CMAKE_SOURCE_DIR}/src/dbStatic
  ${CMAKE_SOURCE_DIR}/src/ca
)

# antelope

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c
  COMMAND ${ANTELOPE} ${CMAKE_CURRENT_SOURCE_DIR}/dbLoadTemplate.y
  DEPENDS dbLoadTemplate.y
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dbLoadTemplate.c
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c ${CMAKE_CURRENT_BINARY_DIR}/dbLoadTemplate.c
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c
)

# lex

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
  COMMAND ${FLEX} -8 -I -S${FLEXSKEL} ${CMAKE_CURRENT_SOURCE_DIR}/dbLoadTemplate_lex.l
  DEPENDS dbLoadTemplate_lex.l
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dbLoadTemplate_lex.c
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c ${CMAKE_CURRENT_BINARY_DIR}/dbLoadTemplate_lex.c
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
)

set_source_files_properties(dbLoadTemplate.c
  PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dbLoadTemplate_lex.c
)


set(LIB_HEADERS
  dbLoadTemplate.h
  dbtoolsIocRegister.h
)

set(LIB_SRCS
  dbLoadTemplate.c
  dbtoolsIocRegister.c
)

set(LIB_DEPS dbIoc dbStaticIoc Com)

add_both_library(dbtoolsIoc
  SONUM ${BASE_SOVERSION}
  SOURCES ${LIB_SRCS}
  LINK_LIBRARIES ${LIB_DEPS}
)

install(FILES ${LIB_HEADERS}
  DESTINATION ${EPICS_INSTALL_INCLUDE}
)
