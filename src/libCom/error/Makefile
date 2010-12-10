#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# This is a Makefile fragment, see src/libCom/Makefile.

SRC_DIRS += $(LIBCOM)/error

INC += epicsPrint.h
INC += errMdef.h
INC += errSymTbl.h
INC += errlog.h
INC += error.h

SRCS += errlog.c
SRCS += errSymLib.c
SRCS += errSymTbl.c

# For bldErrSymTbl
#
ERR_S_FILES += $(LIBCOM)/osi/devLib.h
ERR_S_FILES += $(SRC)/as/asLib.h
ERR_S_FILES += $(SRC)/db/dbAccessDefs.h
ERR_S_FILES += $(SRC)/dbStatic/devSup.h
ERR_S_FILES += $(SRC)/dbStatic/drvSup.h
ERR_S_FILES += $(SRC)/dbStatic/recSup.h
ERR_S_FILES += $(SRC)/dbStatic/dbStaticLib.h
ERR_S_FILES += $(LIBCOM)/error/errMdef.h
ERR_S_FILES += $(SRC)/cas/generic/casdef.h
ERR_S_FILES += $(SRC)/gdd/gddAppFuncTable.h

