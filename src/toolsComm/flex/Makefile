#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../..
 
include $(TOP)/configure/CONFIG
 
HOST_WARN=NO
 
YACC = $(EYACC)
YACCOPT = -l -d

SKELETON_FILE=include/flex.skel.static

#	ARCH_CLASS hp700 needs special flags,
#	everything else uses DEFAULT:
#
USR_CPPFLAGS_DEFAULT := -DUSG
USR_CPPFLAGS_hp700 := -DSCO_UNIX

#	This applies to all arch.classes:
#
USR_CPPFLAGS = -DDEFAULT_SKELETON_FILE="\"$(SKELETON_FILE)\""

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

inc depends: $(INSTALL_PROD)

#	EOF Makefile.Host for base/src/toolsComm/flex
