#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# This is a Makefile fragment, see src/libCom/Makefile.

SRC_DIRS += $(LIBCOM)/yacc

# Yet Another JSON Library
SRC_DIRS += $(LIBCOM)/yajl
INC += yajl_common.h
INC += yajl_gen.h
INC += yajl_parse.h
# The other yajl_*.h files are for internal use only
SRCS += yajl.c
SRCS += yajl_alloc.c
SRCS += yajl_buf.c
SRCS += yajl_encode.c
SRCS += yajl_gen.c
SRCS += yajl_lex.c
SRCS += yajl_parser.c
