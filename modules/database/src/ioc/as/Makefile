#*************************************************************************
# Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2010 Brookhaven Science Associates, as Operator of
#     Brookhaven National Lab.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# This is a Makefile fragment, see src/ioc/Makefile.

SRC_DIRS += $(IOCDIR)/as

INC += asDbLib.h
INC += asCa.h
INC += asIocRegister.h

dbCore_SRCS += asDbLib.c
dbCore_SRCS += asCa.c
dbCore_SRCS += asIocRegister.c

PROD_HOST += ascheck
ascheck_SRCS = ascheck.c
ascheck_LIBS = dbCore ca
