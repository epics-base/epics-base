#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# Copyright (c) 2002 Berliner Elektronenspeicherringgesellschaft fuer
#     Synchrotronstrahlung.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../..

include $(TOP)/configure/CONFIG

SHARED_LIBRARIES = NO

# Build but don't install catools as a static library
LIBRARY += catools
INSTALL_LIBS =

catools_SRCS += tool_lib.c
catools_LIBS += ca Com

# Build and link programs against the catools library
PROD_DEFAULT += caget camonitor cainfo caput
PROD_vxWorks = -nil-
PROD_RTEMS = -nil-
PROD_iOS = -nil-

PROD_LIBS += catools ca Com
catools_DIR = .

include $(TOP)/configure/RULES
