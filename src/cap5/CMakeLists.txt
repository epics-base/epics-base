
execute_process(
  COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/perlConfig.pl ccflags
  OUTPUT_VARIABLE PERLFLAGS
)

execute_process(
  COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/perlConfig.pl archlib
  OUTPUT_VARIABLE PERLARCH
)
find_path(PERLCORE
  NAMES perl.h
  PATHS ${PERLARCH}
  PATH_SUFFIXES CORE
  NO_DEFAULT_PATH
)

execute_process(
  COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/perlConfig.pl privlib
  OUTPUT_VARIABLE PERLPRIV
)
find_path(EXTUTILS
  NAMES xsubpp typemap
  PATHS ${PERLPRIV}
  PATH_SUFFIXES ExtUtils
  NO_DEFAULT_PATH
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Cap5.c
  COMMAND ${PERL_EXECUTABLE} ${EXTUTILS}/xsubpp -typemap ${EXTUTILS}/typemap -output ${CMAKE_CURRENT_BINARY_DIR}/Cap5.c ${CMAKE_CURRENT_SOURCE_DIR}/Cap5.xs
  DEPENDS Cap5.xs
)


if(PERLCORE AND EXTUTILS)

  include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/generic
    ${CMAKE_CURRENT_SOURCE_DIR}/generic/st
    ${CMAKE_CURRENT_SOURCE_DIR}/io/bsdSocket
    ${LIBCOM_DIRS}
    ${CMAKE_SOURCE_DIR}/src/dbStatic
    ${CMAKE_SOURCE_DIR}/src/db
    ${CMAKE_BINARY_DIR}/src/db
    ${CMAKE_SOURCE_DIR}/src/ca
    ${CMAKE_SOURCE_DIR}/src/gdd
    ${CMAKE_BINARY_DIR}/src/gdd
    ${PERLCORE}
  )

  add_library(Cap5 MODULE
    ${CMAKE_CURRENT_BINARY_DIR}/Cap5.c
  )
  target_link_libraries(Cap5
    ComShared
    caShared
  )

  # Manually set dependencies
  # since auto-detection w/ generated
  # files seems to be troublesome
  add_dependencies(Cap5
    dbStaticIocShared
    dbIocShared
  )

  set_target_properties(Cap5
    PROPERTIES COMPILE_FLAGS ${PERLFLAGS}
  )

  install(TARGETS Cap5
    LIBRARY DESTINATION ${EPICS_LIB_DIR}
  )

endif(PERLCORE AND EXTUTILS)
