
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
INC += smartGDDPointer.h
INC += gddApps.h

AITGENSRCS := gdd.cc gddTest.cc gddAppTable.cc gddNewDel.cc \
	gddAppDefs.cc aitTypes.c aitConvert.cc aitHelpers.cc  \
	gddArray.cc gddContainer.cc gddErrorCodes.cc gddUtils.cc

gdd_SRCS := gdd.cc gddTest.cc gddAppTable.cc gddNewDel.cc \
	gddAppDefs.cc aitTypes.c aitConvert.cc aitHelpers.cc dbMapper.cc \
	gddArray.cc gddContainer.cc gddErrorCodes.cc gddUtils.cc \
	smartGDDPointer.cc

LIBRARY = gdd

genApps_SRCS = genApps.cc $(AITGENSRCS)
aitGen_SRCS = aitTypes.c aitGen.c
#TESTPROD = aitGen genApps

include $(TOP)/configure/RULES

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
		$(INSTALL_INCLUDE)/gddApps.h ../aitTypes.h  ../gddUtils.h ../gddUtilsI.h

# Rules for generated files
#
aitConvertGenerated.cc: aitGen$(HOSTEXE) ../aitTypes.h
	./aitGen$(HOSTEXE)

gddApps.h : genApps$(EXE)
	./genApps$(EXE) $@

# unfortunately there is only one TESTPROD/TESTPROD_SRCS allowed
# right now
# (or many 'simple' progs:
#  TESTPROD:=a b c
#  where a is build from a.cc,   b from b.cc, c from c.cc
# )
#
# This rule, however, should be system-independent:
aitGen$(EXE): aitGen$(OBJ) aitTypes$(OBJ)
	$(LINK.c) $^ 

genApps$(EXE): genApps$(OBJ) $(addsuffix $(OBJ), $(basename $(AITGENSRCS)))
	$(LINK.cpp) $^ 

clean::
	$(RM) aitConvertGenerated.cc aitGen$(HOSTEXE) genApps$(EXE)

#	EOF base/src/gdd/Makefile.Host:n

