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

INC += cvtTable.h

DBD += menuConvert.dbd
INC += menuConvert.h

DBD += bptTypeJdegC.dbd
DBD += bptTypeJdegF.dbd
DBD += bptTypeKdegC.dbd
DBD += bptTypeKdegF.dbd

PROD_LIBS = Com
PROD_HOST += makeBpt

makeBpt_SRCS = makeBpt

include $(TOP)/configure/RULES

$(COMMON_DBDS): $(TOOLS)/makeBpt$(HOSTEXE)

