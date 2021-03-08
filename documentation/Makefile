TOP = ..
include $(TOP)/configure/CONFIG

ifeq ($(T_A),$(EPICS_HOST_ARCH))

DOXYGEN=doxygen

EXPAND = Doxyfile@

EXPAND_ME += EPICS_VERSION
EXPAND_ME += EPICS_REVISION
EXPAND_ME += EPICS_MODIFICATION
EXPAND_ME += EPICS_PATCH_LEVEL
EXPAND_ME += OS_CLASS CMPLR_CLASS

ME = documentation/O.$(T_A)/html
GH_FILES = $(ME)/ $(ME)/.nojekyll $(ME)/*.* $(ME)/*/*.*

install: doxygen

doxygen: Doxyfile ../mainpage.dox
	$(DOXYGEN)
	rsync -av $(TOP)/html/ html/

.PHONY: doxygen

commit: doxygen
	$(TOUCH) html/.nojekyll
	(cd $(TOP) && $(CURDIR)/../commit-gh.sh $(GH_FILES))

.PHONY: commit

endif # EPICS_HOST_ARCH

include $(TOP)/configure/RULES
