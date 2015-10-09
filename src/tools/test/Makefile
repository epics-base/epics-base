#*************************************************************************
# Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../..

include $(TOP)/configure/CONFIG

TESTS += Breaktable
TESTS += DBD
TESTS += Device
TESTS += Driver
TESTS += Function
TESTS += macLib
TESTS += Menu
TESTS += Recfield
TESTS += Recordtype
TESTS += Registrar
TESTS += Snippets
TESTS += Variable

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

