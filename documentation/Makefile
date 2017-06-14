TOP = ..
include $(TOP)/configure/CONFIG

ifdef T_A

DOXYGEN=doxygen

EXPAND = Doxyfile

EXPAND_ME += EPICS_VERSION
EXPAND_ME += EPICS_REVISION
EXPAND_ME += EPICS_MODIFICATION
EXPAND_ME += EPICS_PATCH_LEVEL

ME = documentation/O.$(T_A)/html

install: doxygen

doxygen: Doxyfile
	$(DOXYGEN)
	rsync -av $(TOP)/html/ html/

.PHONY: doxygen

commit: doxygen
	$(TOUCH) html/.nojekyll
	(cd $(TOP) && $(CURDIR)/../commit-gh.sh $(ME)/ $(ME)/.nojekyll $(ME)/*.* $(ME)/*/*.*)

.PHONY: commit

endif # T_A

include $(TOP)/configure/RULES
