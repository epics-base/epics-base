TOP = ..
include $(TOP)/configure/CONFIG

EXPAND = Doxyfile@
EXPAND_ME += EPICS_VERSION
EXPAND_ME += EPICS_REVISION
EXPAND_ME += EPICS_MODIFICATION
EXPAND_ME += EPICS_PATCH_LEVEL
EXPAND_ME += OS_CLASS CMPLR_CLASS

HTMLS += ComponentReference.html

TARGETS += doxygen

DOXYGEN = doxygen
ME = documentation/O.$(T_A)/html
GH_FILES = $(ME)/ $(ME)/.nojekyll $(ME)/*.* $(ME)/*/*.*

include $(TOP)/configure/RULES

doxygen: Doxyfile ../mainpage.dox $(INSTALL_HTMLS)
	$(DOXYGEN)
	rsync -av $(TOP)/html/ html/

.PHONY: doxygen

commit: doxygen
	$(TOUCH) html/.nojekyll
	(cd $(TOP) && $(CURDIR)/../commit-gh.sh $(GH_FILES))

.PHONY: commit
