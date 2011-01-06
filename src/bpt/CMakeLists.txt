
include_directories(
  ${LIBCOM_DIRS}
)

set(HEADERS
  cvtTable.h
  menuConvert.h
)

menuDbd(menuConvert.dbd menuConvert.h)

makeBpt(bptTypeJdegC.data bptTypeJdegC.dbd bptTypeJdegC.h)

makeBpt(bptTypeJdegF.data bptTypeJdegF.dbd bptTypeJdegF.h)

makeBpt(bptTypeKdegC.data bptTypeKdegC.dbd bptTypeKdegC.h)

makeBpt(bptTypeKdegF.data bptTypeKdegF.dbd bptTypeKdegF.h)

if(HOST)
  add_executable(makeBpt
    makeBpt.c
  )

  install(TARGETS makeBpt
    RUNTIME DESTINATION ${EPICS_INSTALL_BIN}
  )

endif(HOST)

add_custom_target(dbdbd
  DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/menuConvert.h
    ${CMAKE_CURRENT_BINARY_DIR}/bptTypeJdegC.h
    ${CMAKE_CURRENT_BINARY_DIR}/bptTypeJdegF.h
    ${CMAKE_CURRENT_BINARY_DIR}/bptTypeKdegC.h
    ${CMAKE_CURRENT_BINARY_DIR}/bptTypeKdegF.h
    dbIoc${UTIL}
)
set_target_properties(dbdbd
  PROPERTIES EXCLUDE_FROM_ALL NO
)

install(FILES
    cvtTable.h
    ${CMAKE_CURRENT_BINARY_DIR}/menuConvert.h
    ${CMAKE_CURRENT_BINARY_DIR}/bptTypeJdegC.h
    ${CMAKE_CURRENT_BINARY_DIR}/bptTypeJdegF.h
    ${CMAKE_CURRENT_BINARY_DIR}/bptTypeKdegC.h
    ${CMAKE_CURRENT_BINARY_DIR}/bptTypeKdegF.h
  DESTINATION ${EPICS_INSTALL_INCLUDE}
)

install(FILES
    menuConvert.dbd
    ${CMAKE_CURRENT_BINARY_DIR}/bptTypeJdegC.dbd
    ${CMAKE_CURRENT_BINARY_DIR}/bptTypeJdegF.dbd
    ${CMAKE_CURRENT_BINARY_DIR}/bptTypeKdegC.dbd
    ${CMAKE_CURRENT_BINARY_DIR}/bptTypeKdegF.dbd
  DESTINATION ${EPICS_INSTALL_DBD}
)
