#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# This is a Makefile fragment, see src/libCom/Makefile.

SRC_DIRS += $(LIBCOM)/iocsh
INC += iocsh.h
INC += initHooks.h
INC += registry.h
INC += libComRegister.h
Com_SRCS += iocsh.cpp
Com_SRCS += initHooks.c
Com_SRCS += registry.c
Com_SRCS += libComRegister.c

iocsh_CXXFLAGS += -DEPICS_COMMANDLINE_LIBRARY=EPICS_COMMANDLINE_LIBRARY_$(COMMANDLINE_LIBRARY)
iocsh_INCLUDES += $(INCLUDES_$(COMMANDLINE_LIBRARY))
