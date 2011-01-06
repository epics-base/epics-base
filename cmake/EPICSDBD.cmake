#
# Macros for processing EPICS DataBase Definition (DBD) files
#

macro(menuDbd DBD HFILE)
  add_custom_command(OUTPUT ${HFILE}
    COMMAND ${DBTOMENUH} ${DBDFLAGS} ${CMAKE_CURRENT_SOURCE_DIR}/${DBD} ${HFILE}
    DEPENDS ${DBD} ${DBTOMENUH}
  )
endmacro(menuDbd)

macro(makeBpt DATA DBD HFILE)
  add_custom_command(OUTPUT ${DBD}
    COMMAND ${MAKEBPT} ${CMAKE_CURRENT_SOURCE_DIR}/${DATA} ${CMAKE_CURRENT_BINARY_DIR}/${DBD}
    DEPENDS ${DATA} ${MAKEBPT}
  )
  add_custom_command(OUTPUT ${HFILE}
    COMMAND ${DBTOMENUH} ${DBDFLAGS} ${DBD} ${HFILE}
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${DBD} ${DBTOMENUH}
  )
endmacro(makeBpt)

macro(recordType DBD HFILE)
  add_custom_command(OUTPUT ${HFILE}
    COMMAND ${DBTORECORDTYPEH} ${DBDFLAGS} ${CMAKE_CURRENT_SOURCE_DIR}/${DBD} ${HFILE}
    DEPENDS ${DBD} ${DBTORECORDTYPEH}
  )
endmacro(recordType)

# TODO: Recursive dependency scanner?
macro(expandDbd OUTFILE)
  add_custom_command(OUTPUT ${OUTFILE}
    COMMAND ${DBEXPAND} ${DBDFLAGS} -o ${OUTFILE} ${ARGN}
    DEPENDS ${ARGN} ${DBEXPAND}
  )
endmacro(expandDbd)

macro(registerRDD IN OUTFN)
  add_custom_command(OUTPUT ${OUTFN}.cpp
    COMMAND ${PERL_EXECUTABLE} ${CMAKE_SOURCE_DIR}/src/registry/registerRecordDeviceDriver.pl ${IN} ${OUTFN} ${CMAKE_INSTALL_PREFIX} ${OUTFN}.cpp
    DEPENDS ${IN} ${CMAKE_SOURCE_DIR}/src/registry/registerRecordDeviceDriver.pl
  )
endmacro(registerRDD)
