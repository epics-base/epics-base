#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

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
INC += gddEnumStringTable.h

HTMLS += gdd.html
HTMLS += gddref.html
HTMLS += gddref2.html

AITGENSRCS := gdd.cc gddTest.cc gddAppTable.cc gddNewDel.cc \
	gddAppDefs.cc aitTypes.c aitConvert.cc aitHelpers.cc  \
	gddArray.cc gddContainer.cc gddErrorCodes.cc gddUtils.cc \
    gddEnumStringTable.cc

gdd_SRCS := gdd.cc gddTest.cc gddAppTable.cc gddNewDel.cc \
	gddAppDefs.cc aitTypes.c aitConvert.cc aitHelpers.cc dbMapper.cc \
	gddArray.cc gddContainer.cc gddErrorCodes.cc gddUtils.cc \
	gddEnumStringTable.cc

LIBRARY = gdd

gdd_LIBS = Com

gdd_RCS_WIN32 = gdd.rc

PROD_HOST = aitGen genApps
genApps_SRCS = genApps.cc $(AITGENSRCS)
aitGen_SRCS = aitTypes.c aitGen.c
PROD_LIBS = Com

# aitGen.c doesn't compile for linux-arm at -O3 when using gcc-3.4.5
aitGen_CFLAGS_linux-arm = -O2

# Switch off potentially bogus warnings on HPUX 11 - detailed warning
# suppression in the source code would be too much effort with respect
# to gdd's limited future

HPWARNFLAGS_NO = +W361 +W392 +W655 +W749 +W818 +W930
USR_CXXFLAGS_hpux = $(HPWARNFLAGS_$(GNU))

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
aitConvertGenerated.cc: $(INSTALL_HOST_BIN)/aitGen$(HOSTEXE) aitTypes.h
	$(INSTALL_HOST_BIN)/aitGen$(HOSTEXE)

$(COMMON_DIR)/gddApps.h : $(INSTALL_HOST_BIN)/genApps$(HOSTEXE)
	$(INSTALL_HOST_BIN)/genApps$(HOSTEXE) $@

clean::
	$(RM) aitConvertGenerated.cc gddApps.h

aitConvert$(OBJ): aitConvertGenerated.cc

#	EOF base/src/gdd/Makefile.Host:n

