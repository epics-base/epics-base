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

Com_SRCS += errlog.c
Com_SRCS += errSymLib.c
Com_SRCS += errSymTbl.c

# For bldErrSymTbl
#
ERR_S_FILES += $(LIBCOM)/osi/devLib.h
ERR_S_FILES += $(LIBCOM)/osi/epicsTime.h
ERR_S_FILES += $(LIBCOM)/as/asLib.h
ERR_S_FILES += $(LIBCOM)/misc/epicsStdlib.h
ERR_S_FILES += $(LIBCOM)/pool/epicsThreadPool.h
ERR_S_FILES += $(SRC)/ioc/db/dbAccessDefs.h
ERR_S_FILES += $(SRC)/ioc/dbStatic/devSup.h
ERR_S_FILES += $(SRC)/ioc/dbStatic/drvSup.h
ERR_S_FILES += $(SRC)/ioc/dbStatic/recSup.h
ERR_S_FILES += $(SRC)/ioc/dbStatic/dbStaticLib.h
ERR_S_FILES += $(LIBCOM)/error/errMdef.h
ERR_S_FILES += $(SRC)/ca/legacy/pcas/generic/casdef.h
ERR_S_FILES += $(SRC)/ca/legacy/gdd/gddAppFuncTable.h

CLEANS += errSymTbl.c
