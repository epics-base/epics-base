#*************************************************************************
# Copyright (c) 2010 Brookhaven Science Associates, as Operator of
#     Brookhaven National Lab.
# Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#************************************************************************
TOP=../../..
include $(TOP)/configure/CONFIG

INC += asLib.h
INC += asTrapWrite.h

LIB_SRCS += asLib.c
LIB_SRCS += asTrapWrite.c

LIBRARY_HOST = asHost

asHost_LIBS  = Com

asHost_RCS = asHost.rc

include $(TOP)/configure/RULES

# Extra rule since asLib_lex.c is included by asLib.c
asLib$(OBJ): asLib_lex.c

clean::
	@$(RM) asLib.c asLib_lex.c
