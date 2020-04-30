#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************
LIBCOM := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
TOP = ../../..

include $(TOP)/configure/CONFIG

# Uncomment this to remove the (benign) valgrind helper stubs
#USR_CFLAGS += -DNVALGRIND

INC += valgrind/valgrind.h

EXPAND_COMMON = libComVersion.h@

EXPAND_ME += EPICS_LIBCOM_MAJOR_VERSION
EXPAND_ME += EPICS_LIBCOM_MINOR_VERSION
EXPAND_ME += EPICS_LIBCOM_MAINTENANCE_VERSION
EXPAND_ME += EPICS_LIBCOM_DEVELOPMENT_FLAG

INC += libComVersion.h

include $(LIBCOM)/as/Makefile
include $(LIBCOM)/bucketLib/Makefile
include $(LIBCOM)/calc/Makefile
include $(LIBCOM)/cvtFast/Makefile
include $(LIBCOM)/cppStd/Makefile
include $(LIBCOM)/cxxTemplates/Makefile
include $(LIBCOM)/dbmf/Makefile
include $(LIBCOM)/ellLib/Makefile
include $(LIBCOM)/env/Makefile
include $(LIBCOM)/error/Makefile
include $(LIBCOM)/fdmgr/Makefile
include $(LIBCOM)/flex/Makefile
include $(LIBCOM)/freeList/Makefile
include $(LIBCOM)/gpHash/Makefile
include $(LIBCOM)/iocsh/Makefile
include $(LIBCOM)/log/Makefile
include $(LIBCOM)/macLib/Makefile
include $(LIBCOM)/misc/Makefile
include $(LIBCOM)/osi/Makefile
include $(LIBCOM)/pool/Makefile
include $(LIBCOM)/ring/Makefile
include $(LIBCOM)/taskwd/Makefile
include $(LIBCOM)/timer/Makefile
include $(LIBCOM)/yacc/Makefile
include $(LIBCOM)/yajl/Makefile

# Generate library API header file
API_HEADER = libComAPI.h
Com_API = libCom

# Library to build:
LIBRARY=Com

Com_SYS_LIBS_WIN32 = ws2_32 advapi32 user32 dbghelp

Com_RCS = Com.rc

ifeq ($(T_A),$(EPICS_HOST_ARCH))
  # Antelope & flex are needed to finish libCom
  DELAY_INSTALL_LIBS = YES
endif

# shared library ABI version.
SHRLIB_VERSION = $(EPICS_LIBCOM_MAJOR_VERSION).$(EPICS_LIBCOM_MINOR_VERSION).$(EPICS_LIBCOM_MAINTENANCE_VERSION)

include $(TOP)/configure/RULES

include $(LIBCOM)/as/RULES
include $(LIBCOM)/env/RULES
include $(LIBCOM)/error/RULES
include $(LIBCOM)/flex/RULES
include $(LIBCOM)/misc/RULES
include $(LIBCOM)/osi/RULES
include $(LIBCOM)/yajl/RULES
