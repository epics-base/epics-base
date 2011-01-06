#
# A Macro to build both shared and static libraries
# for the same set of sources.
#
# Macros
#   add_both_library(TARGET
#           SONUM X.Y
#           SOURCES <files>
#           LINK_LIBRARIES <epicslibs>
#           EXTRA_LIB <otherlib>
#           EXTRA_DEB <cmaketargets>
#   )
#
# Note: when linking it is necessary to specify which
#   version to use.  Two targets are created for each
#   library TARGETStatic and TARGETShared.
#

include(MacroArguments)

macro(add_both_library TARG)
  PARSE_ARGUMENTS(LIB
    "SOURCES;LINK_LIBRARIES;EXTRA_LIB;EXTRA_DEP;SONUM"
    ""
    ${ARGN}
  )

  set(libStatic "")
  set(libShared "")
  if(LIB_LINK_LIBRARIES)
    foreach(llib ${LIB_LINK_LIBRARIES})
      list(APPEND libStatic ${llib}Static)
      list(APPEND libShared ${llib}Shared)
    endforeach(llib)
  endif(LIB_LINK_LIBRARIES)

  add_library(${TARG}Static STATIC ${LIB_SOURCES})
  target_link_libraries(${TARG}Static ${libStatic} ${LIB_EXTRA_LIB})
  if(LIB_EXTRA_DEP)
    add_dependencies(${TARG}Static ${LIB_EXTRA_DEP})
  endif(LIB_EXTRA_DEP)

  set_target_properties(${TARG}Static
    PROPERTIES
      OUTPUT_NAME ${TARG}
      CLEAN_DIRECT_OUTPUT 1 # Prevent cmake from deleteing
                            # whichever version is build first
                            # when building the second
  )
  if(WIN32)
    set_target_properties(${TARG}Static
       PROPERTIES
         COMPILE_DEFINITIONS EPICS_DLL_NO
    )
  endif(WIN32)

  install(TARGETS ${TARG}Static
    ARCHIVE DESTINATION ${EPICS_LIB_DIR}
  )

  if(BUILD_SHARED_LIBS)

    add_library(${TARG}Shared SHARED ${LIB_SOURCES})
    target_link_libraries(${TARG}Shared ${libShared} ${LIB_EXTRA_LIB})
    if(LIB_EXTRA_DEP)
      add_dependencies(${TARG}Shared ${LIB_EXTRA_DEP})
    endif(LIB_EXTRA_DEP)

    set_target_properties(${TARG}Shared
      PROPERTIES
        OUTPUT_NAME ${TARG}
        SOVERSION ${LIB_SONUM}
        CLEAN_DIRECT_OUTPUT 1
    )
    if(WIN32)
      set_target_properties(${TARG}Shared
         PROPERTIES
           COMPILE_DEFINITIONS _DLL
      )
    endif(WIN32)

    install(TARGETS ${TARG}Shared
      ARCHIVE DESTINATION ${EPICS_LIB_DIR}
      LIBRARY DESTINATION ${EPICS_LIB_DIR}
      RUNTIME DESTINATION ${EPICS_BIN_DIR}
    )

    add_custom_target(${TARG}
      DEPENDS ${TARG}Static ${TARG}Shared
    )
  else(BUILD_SHARED_LIBS)

    add_custom_target(${TARG}
      DEPENDS ${TARG}Static
    )
  endif(BUILD_SHARED_LIBS)
endmacro(add_both_library)
