
TOP=../..
include $(TOP)/configure/CONFIG

INC += gdd.h
INC += gddI.h
INC += gddContainer.h
INC += gddContainerI.h
INC += gddArray.h
INC += gddArrayI.h
INC += gddScalar.h
INC += gddScalarI.h
INC += gddSemaphore.h
INC += gddNewDel.h
INC += gddUtils.h
INC += gddUtilsI.h
INC += gddErrorCodes.h
INC += aitTypes.h
INC += aitConvert.h
INC += aitHelpers.h
INC += dbMapper.h
INC += gddAppTable.h
INC += gddAppFuncTable.h
INC += gddApps.h
INC += smartGDDPointer.h

AITGENSRCS := gdd.cc gddTest.cc gddAppTable.cc gddNewDel.cc \
	gddAppDefs.cc aitTypes.c aitConvert.cc aitHelpers.cc  \
	gddArray.cc gddContainer.cc gddErrorCodes.cc gddUtils.cc

LIBSRCS := gdd.cc gddTest.cc gddAppTable.cc gddNewDel.cc \
	gddAppDefs.cc aitTypes.c aitConvert.cc aitHelpers.cc dbMapper.cc \
	gddArray.cc gddContainer.cc gddErrorCodes.cc gddUtils.cc \
	smartGDDPointer.cc

LIBRARY_DEFAULT = gdd

include $(TOP)/configure/RULES_BUILD

# cannot generate these dependencies automatically
#
# Problem: Some dependencies are include files that may
#	not have been installed if we are building for
#	the first time
#	-> use explicit reference to the uninstalled files in '..'
aitConvert$(OBJ): ../aitConvert.cc aitConvertGenerated.cc\
		../aitConvert.h ../aitTypes.h


dbMapper.o:	../gdd.h ../gddI.h ../gddContainer.h ../gddContainerI.h \
		../gddArray.h ../gddScalar.h ../gddAppTable.h ../dbMapper.h \
		gddApps.h ../aitTypes.h  ../gddUtils.h ../gddUtilsI.h

# Rules for generated files
#
aitConvertGenerated.cc:  $(EPICS_BASE_HOST_BIN)/aitGen$(HOSTEXE) ../aitTypes.h
	 $(EPICS_BASE_HOST_BIN)/aitGen$(HOSTEXE)

gddApps.h :  $(EPICS_BASE_HOST_BIN)/genApps$(HOSTEXE)
	 $(EPICS_BASE_HOST_BIN)/genApps$(HOSTEXE) $@

# unfortunately there is only one TESTPROD/TESTPROD_SRCS allowed
# right now
# (or many 'simple' progs:
#  TESTPROD:=a b c
#  where a is build from a.cc,   b from b.cc, c from c.cc
# )
#
# This rule, however, should be system-independent:
ifeq ($(HOST_ARCH),$(T_A))

aitGen$(HOSTEXE): aitGen$(OBJ) aitTypes$(OBJ)
	$(LINK.c) $^ 

genApps$(HOSTEXE): genApps$(OBJ) $(addsuffix $(OBJ), $(basename $(AITGENSRCS)))
	$(LINK.cc) $^ 

endif

clean::
	$(RM) aitConvertGenerated.cc aitGen$(HOSTEXE) genApps$(HOSTEXE)

#	EOF base/src/gdd/Makefile
