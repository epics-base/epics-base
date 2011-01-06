
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${LIBCOM_DIRS}
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c ${CMAKE_CURRENT_BINARY_DIR}/y.tab.h
  COMMAND ${ANTELOPE} -l -d ${CMAKE_CURRENT_SOURCE_DIR}/parse.y
  DEPENDS parse.y
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/parse.c
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c ${CMAKE_CURRENT_BINARY_DIR}/parse.c
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/parse.h
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/y.tab.h ${CMAKE_CURRENT_BINARY_DIR}/parse.h
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/y.tab.h
)

add_executable(e_flex
  ccl.c
  dfa.c
  ecs.c
  gen.c
  misc.c
  nfa.c
  sym.c
  tblcmp.c
  ${CMAKE_CURRENT_BINARY_DIR}/parse.c
  ${CMAKE_CURRENT_BINARY_DIR}/parse.h
)
target_link_libraries(e_flex
  Com${UTIL}
)
set_target_properties(e_flex
  PROPERTIES HAS_CXX Yes
)

install(TARGETS e_flex
  RUNTIME DESTINATION ${EPICS_BIN_DIR}
)
install(FILES flex.skel.static
  DESTINATION ${EPICS_INCLUDE_DIR}
)
