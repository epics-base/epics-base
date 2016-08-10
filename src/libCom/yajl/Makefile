#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
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
INC += yajl_alloc.h
INC += yajl_common.h
INC += yajl_gen.h
INC += yajl_parse.h
# The other yajl_*.h files are for internal use only

Com_SRCS += yajl.c
Com_SRCS += yajl_alloc.c
Com_SRCS += yajl_buf.c
Com_SRCS += yajl_encode.c
Com_SRCS += yajl_gen.c
Com_SRCS += yajl_lex.c
Com_SRCS += yajl_parser.c
