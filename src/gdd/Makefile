#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
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

gdd_RCS = gdd.rc

PROD_HOST = aitGen genApps
genApps_SRCS = genApps.cc $(AITGENSRCS)
aitGen_SRCS = aitTypes.c aitGen.c
PROD_LIBS = Com
genApps_SYS_LIBS_WIN32 = ws2_32

# aitGen.c doesn't compile for linux-arm at -O3 when using gcc-3.4.5
aitGen_CFLAGS_linux-arm = -O2

USR_CXXFLAGS_Linux = -fno-strict-aliasing
USR_CXXFLAGS_RTEMS = -fno-strict-aliasing

include $(TOP)/configure/RULES

# cannot generate these dependencies automatically
#
# Problem: Some dependencies are include files that may
#	not have been installed if we are building for
#	the first time
#	-> use explicit reference to the uninstalled files in '..'

aitConvert$(OBJ): $(COMMON_DIR)/aitConvertGenerated.cc

dbMapper$(OBJ):	$(COMMON_DIR)/gddApps.h 

# Rules for generated files

$(COMMON_DIR)/aitConvertGenerated.cc: $(TOOLS)/aitGen$(HOSTEXE)
	$(TOOLS)/aitGen$(HOSTEXE) $@

$(COMMON_DIR)/gddApps.h : $(TOOLS)/genApps$(HOSTEXE)
	$(TOOLS)/genApps$(HOSTEXE) $@

clean::
	@$(RM) $(COMMON_DIR)/aitConvertGenerated.cc

#	EOF base/src/gdd/Makefile

