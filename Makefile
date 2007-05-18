#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
# National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
# Operator of Los Alamos National Laboratory.
# This file is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************
#
# $Id$
#
TOP = ../..

include $(TOP)/configure/CONFIG

PROD_HOST = msi
HTMLS = msi.html

msi_SRCS = msi.c
PROD_LIBS += Com

include $(TOP)/configure/RULES
