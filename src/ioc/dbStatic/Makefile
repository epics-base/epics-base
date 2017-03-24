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

dbCore_SRCS += dbStaticLib.c
dbCore_SRCS += dbYacc.c
dbCore_SRCS += dbPvdLib.c
dbCore_SRCS += dbStaticRun.c
dbCore_SRCS += dbStaticIocRegister.c

CLEANS += dbLex.c dbYacc.c
