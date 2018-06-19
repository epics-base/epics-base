#*************************************************************************
# Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

# This is a Makefile fragment, see src/libCom/Makefile.

SRC_DIRS += $(LIBCOM)/flex

parse_YACCOPT = -l -d

SKELETON_FILE = flex.skel.static

parse_CPPFLAGS = -DDEFAULT_SKELETON_FILE=$(SKELETON_FILE)

INC += flex.skel.static

# flex.c is included in parse.c
e_flex_SRCS += ccl.c
e_flex_SRCS += dfa.c
e_flex_SRCS += ecs.c
e_flex_SRCS += gen.c
e_flex_SRCS += misc.c
e_flex_SRCS += nfa.c
e_flex_SRCS += sym.c
e_flex_SRCS += tblcmp.c
e_flex_SRCS += parse.c
e_flex_OBJS += epicsTempFile$(OBJ)

PROD_HOST += e_flex

CLEANS += parse.c parse.h
