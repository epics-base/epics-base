#*************************************************************************
# Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# This is a Makefile fragment, see src/ioc/Makefile.

SRC_DIRS += $(IOCDIR)/dbStatic

INC += dbBase.h
INC += dbFldTypes.h
INC += dbStaticLib.h
INC += dbStaticPvt.h
INC += link.h
INC += special.h
INC += guigroup.h
INC += devSup.h
INC += drvSup.h
INC += recSup.h
INC += dbStaticIocRegister.h

STATIC_SRCS += dbStaticLib.c
STATIC_SRCS += dbYacc.c
STATIC_SRCS += dbPvdLib.c

dbCore_SRCS += $(STATIC_SRCS)
dbCore_SRCS += dbStaticRun.c
dbCore_SRCS += dbStaticIocRegister.c

dbStaticHost_SRCS += $(STATIC_SRCS)
dbStaticHost_SRCS += dbStaticNoRun.c 

LIBRARY_HOST += dbStaticHost

dbStaticHost_LIBS = Com

PROD_HOST += dbReadTest dbExpand dbToMenuH dbToRecordtypeH

dbReadTest_SRCS = dbReadTest.c
dbExpand_SRCS = dbExpand.c
dbToMenuH_SRCS = dbToMenuH.c
dbToRecordtypeH_SRCS = dbToRecordtypeH.c

# Include dbStaticHost objects directly in executables
# because of a Circular dependency induced by a rule
#  $(INSTALL_LIBS): $(INSTALL_SHRLIBS)
# in RULES_BUILD
dbReadTest_SRCS += $(dbStaticHost_SRCS)
dbExpand_SRCS += $(dbStaticHost_SRCS)
dbToMenuH_SRCS += $(dbStaticHost_SRCS)
dbToRecordtypeH_SRCS += $(dbStaticHost_SRCS)

CLEANS += dbLex.c dbYacc.c
