#
# $Id$
#

TOP = .
include $(TOP)/configure/CONFIG

DIRS += config configure src

include $(TOP)/configure/RULES_TOP

release::
	@echo TOP: Creating Release...
	@./MakeRelease

built_release::
	@echo TOP: Creating Fully Built Release...
	@./MakeRelease -b $(INSTALL_LOCATION)

