
include(MacroArguments)

macro(add_ioc_executable IOC_NAME)
  PARSE_ARGUMENTS(IOC
    "SOURCES;LINK_LIBRARIES"
    "SKIP_MUNCH;SKIP_INSTALL"
    ${ARGN}
  )

  if(HOST OR RTEMS)
    # Build a statically linked executable

    add_executable(${IOC_NAME} ${IOC_SOURCES})
    target_link_libraries(${IOC_NAME}
      ${IOC_LINK_LIBRARIES}
    )
    if(WIN32 AND BUILD_SHARED_LIBS)
      set_target_properties(${IOC_NAME}
         PROPERTIES
           COMPILE_DEFINITIONS _DLL
      )
    endif(WIN32 AND BUILD_SHARED_LIBS)

    if(NOT IOC_SKIP_INSTALL)
      install(TARGETS ${IOC_NAME}
        RUNTIME DESTINATION ${EPICS_BIN_DIR}
      )
    endif(NOT IOC_SKIP_INSTALL)

    if(NEED_MUNCH AND NOT SKIP_MUNCH)
      add_custom_target(${IOC_NAME}munch ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${IOC_NAME}.boot
      )
      add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${IOC_NAME}.boot
        COMMAND ${CMAKE_OBJCOPY} -O binary ${IOC_NAME}${CMAKE_EXECUTABLE_SUFFIX} ${IOC_NAME}.boot
        DEPENDS ${IOC_NAME}
      )
      install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${IOC_NAME}.boot
        DESTINATION ${EPICS_BIN_DIR}
      )
    endif(NEED_MUNCH AND NOT SKIP_MUNCH)
  endif(HOST OR RTEMS)

endmacro(add_ioc_executable)
