#
# Top Level EPICS Makefile
#        by Matthew Needes and Mike Bordua
#
#  Notes:
#    The build, clean, install, and depends "commands" do not have
#         their own dependency lists; they are instead handled by
#         the build.%, clean.%, etc. dependencies.
#
#    However, the release dependencies DOES require a complete
#         install because the release.% syntax is illegal.
#
# $Id$
#

TOP=.
include $(TOP)/configure/CONFIG

DIRS = src

include $(TOP)/configure/RULES_TOP

release: 
	@echo TOP: Creating Release...
	@./MakeRelease

built_release:
	@echo TOP: Creating Fully Built Release...
	@./MakeRelease -b $(INSTALL_LOCATION)

