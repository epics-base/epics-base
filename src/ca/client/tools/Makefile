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
TOP=../../../..

include $(TOP)/configure/CONFIG

PROD_DEFAULT += caget camonitor cainfo caput
PROD_vxWorks = -nil-
PROD_RTEMS = -nil-
PROD_iOS = -nil-

PROD_SRCS = tool_lib.c

caget_SRCS = caget.c
caput_SRCS = caput.c
camonitor_SRCS = camonitor.c
cainfo_SRCS = cainfo.c

PROD_LIBS = ca Com

include $(TOP)/configure/RULES
