TOP = ..
include $(TOP)/configure/CONFIG

EXPAND = Doxyfile@
EXPAND_ME += EPICS_VERSION
EXPAND_ME += EPICS_REVISION
EXPAND_ME += EPICS_MODIFICATION
EXPAND_ME += EPICS_PATCH_LEVEL
EXPAND_ME += OS_CLASS CMPLR_CLASS

TARGETS += doxygen

DOXYGEN = doxygen

include $(TOP)/configure/RULES

ifndef T_A
doxygen: all
else
doxygen: Doxyfile
	$(DOXYGEN)
endif

.PHONY: doxygen
