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
include $(TOP)/config/CONFIG_BASE

#
# Directories to build defined in CONFIG_BASE
#

include $(TOP)/config/RULES_DIRS

uninstall:: $(foreach arch, $(BUILD_ARCHS), $(arch)$(DIVIDER)uninstall) 
%$(DIVIDER)uninstall :
	@rm -rf $(INSTALL_LOCATION_BIN)/$* $(INSTALL_LOCATION_LIB)/$* \
		$(INSTALL_LOCATION)/dbd $(INSTALL_MAN) $(INSTALL_INCLUDE) 
	@rm -rf  rec.bak rec
	@DIR1=`pwd`;cd $(INSTALL_LOCATION);DIR2=`pwd`;cd $$DIR1;\
	if [ "$$DIR1" != "$$DIR2" ]; then rm -fr $(INSTALL_LOCATION)/config; fi

release: 
	@echo TOP: Creating Release...
	@./MakeRelease ${TOP}

built_release: install
	@echo TOP: Creating Fully Built Release...
	@./MakeRelease ${TOP} -b

