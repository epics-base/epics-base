#*************************************************************************
# Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# This is a Makefile fragment, see src/ioc/Makefile.

SRC_DIRS += $(IOCDIR)/bpt

INC += cvtTable.h

DBDINC += menuConvert

BPT_DBD += bptTypeJdegC.dbd
BPT_DBD += bptTypeJdegF.dbd
BPT_DBD += bptTypeKdegC.dbd
BPT_DBD += bptTypeKdegF.dbd
DBD += $(BPT_DBD)

PROD_HOST += makeBpt

makeBpt_SRCS = makeBpt

HTMLS += menuConvert.html

