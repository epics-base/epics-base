#*************************************************************************
# Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../..
include $(TOP)/configure/CONFIG

INC += asLib.h
INC += asDbLib.h
INC += asCa.h
INC += asTrapWrite.h
INC += asIocRegister.h

LIB_SRCS += asLib.c
LIB_SRCS += asTrapWrite.c

asIoc_SRCS += asDbLib.c
asIoc_SRCS += asCa.c
asIoc_SRCS += asIocRegister.c

LIBRARY_HOST = asHost
LIBRARY_IOC = asIoc

asHost_LIBS  = dbStaticHost ca Com
asIoc_LIBS  = dbIoc dbStaticIoc ca Com

asHost_RCS = asHost.rc
asIoc_RCS = asIoc.rc

PROD_HOST = ascheck
ascheck_SRCS = ascheck.c
PROD_LIBS = asHost dbStaticHost ca Com

# For R3.13 compatibility only
ifeq ($(strip $(COMPAT_313)),YES)
OBJLIB_vxWorks=asIoc
OBJLIB_SRCS += $(LIB_SRCS) $(asIoc_SRCS)
endif

include $(TOP)/configure/RULES

# Extra rule since asLib_lex.c is included by asLib.c
asLib$(OBJ): asLib_lex.c

clean::
	@$(RM) asLib.c asLib_lex.c

