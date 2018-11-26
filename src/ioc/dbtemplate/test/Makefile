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
dbltExpand_LIBS += dbCore ca Com

TESTS += msi

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

ifneq (,$(findstring $(T_A),$(EPICS_HOST_ARCH) $(CROSS_COMPILER_RUNTEST_ARCHS)))
  TARGETS += msi-copy$(EXE)
endif

include $(TOP)/configure/RULES

msi-copy$(EXE): $(INSTALL_BIN)/msi$(EXE)
	@$(RM) $@
	$(CP) $< $@
