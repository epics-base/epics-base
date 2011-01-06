
include_directories(
  ${LIBCOM_DIRS}
)

add_executable(antelope
  closure.c
  error.c
  lalr.c
  lr0.c
  main.c
  mkpar.c
  output.c
  reader.c
  skeleton.c
  symtab.c
  verbose.c
  warshall.c
)
target_link_libraries(antelope
  Com${UTIL}
)
set_target_properties(antelope
  PROPERTIES HAS_CXX Yes
)

install(TARGETS antelope
  RUNTIME DESTINATION ${EPICS_INSTALL_BIN}
)
