
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

HTMLS += gdd.html
HTMLS += gddref.html
HTMLS += gddref2.html

AITGENSRCS := gdd.cc gddTest.cc gddAppTable.cc gddNewDel.cc \
	gddAppDefs.cc aitTypes.c aitConvert.cc aitHelpers.cc  \
	gddArray.cc gddContainer.cc gddErrorCodes.cc gddUtils.cc

gdd_SRCS := gdd.cc gddTest.cc gddAppTable.cc gddNewDel.cc \
	gddAppDefs.cc aitTypes.c aitConvert.cc aitHelpers.cc dbMapper.cc \
	gddArray.cc gddContainer.cc gddErrorCodes.cc gddUtils.cc \
	smartGDDPointer.cc

LIBRARY_HOST = gdd

DLL_LIBS = Com

genApps_SRCS = genApps.cc $(AITGENSRCS)
aitGen_SRCS = aitTypes.c aitGen.c
PROD = aitGen genApps
PROD_vxWorks = genApps

gdd_RCS_WIN32 = gdd.rc

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
aitConvertGenerated.cc: $(EPICS_BASE_HOST_BIN)/aitGen$(HOSTEXE) ../aitTypes.h
	$(EPICS_BASE_HOST_BIN)/aitGen$(HOSTEXE)

gddApps.h : $(EPICS_BASE_HOST_BIN)/genApps$(HOSTEXE)
	$(EPICS_BASE_HOST_BIN)/genApps$(HOSTEXE) $@

clean::
	$(RM) aitConvertGenerated.cc gddApps.h

#	EOF base/src/gdd/Makefile.Host:n

