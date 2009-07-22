#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../..

include $(TOP)/configure/CONFIG

HOST_WARN=NO

YACC = $(EYACC)
YACCOPT = -l -d

SKELETON_FILE = include/flex.skel.static

USR_CPPFLAGS = -DDEFAULT_SKELETON_FILE=$(SKELETON_FILE)

INC += flex.skel.static

# main.c is included in parse.c
#
SRCS += ccl.c
SRCS += dfa.c
SRCS += ecs.c
SRCS += gen.c
SRCS += misc.c
SRCS += nfa.c
SRCS += sym.c
SRCS += tblcmp.c
SRCS += parse.c

PROD_HOST = e_flex
PROD_LIBS = Com

include $(TOP)/configure/RULES

clean::
	$(RM) parse.c parse.h

#	EOF Makefile.Host for base/src/toolsComm/flex
