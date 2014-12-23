#*************************************************************************
# Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in the file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../../../..

include $(TOP)/configure/CONFIG

TESTPROD_HOST_DEFAULT = dbltExpand
TESTPROD_HOST_WIN32 = -nil-
dbltExpand_SRCS += dbltExpand.c
dbltExpand_LIBS += dbCore Com

TESTS += msi

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

TARGETS_HOST += msi-copy$(EXE)
TARGETS += $(TARGETS_$(BUILD_CLASS))

include $(TOP)/configure/RULES

msi-copy$(EXE): $(INSTALL_BIN)/msi$(EXE)
	@$(RM) $@
	$(CP) $< $@
