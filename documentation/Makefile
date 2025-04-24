TOP = ..
include $(TOP)/configure/CONFIG

EXPAND = Doxyfile@
EXPAND_ME += EPICS_VERSION
EXPAND_ME += EPICS_REVISION
EXPAND_ME += EPICS_MODIFICATION
EXPAND_ME += EPICS_PATCH_LEVEL
EXPAND_ME += OS_CLASS CMPLR_CLASS
EXPAND_ME += RTD_SRC

DOXYGEN ?= doxygen

include $(TOP)/documentation/Makefile.headers

HEADER_MD_FILES = $(foreach t, $(HEADER_TYPES), \
    $(addsuffix _h.md, $($t_HEADERS)))
API_RST_FILES = $(addsuffix -api.rst, $(HEADER_TYPES))

RTD_SRC = $(COMMON_DIR)/rtd-src

DOCS += README.md
DOCS += ca-cli.md
DOCS += RELEASE_NOTES.md

OLD_NOTES = $(wildcard ../RELEASE-*.md)
DOCS += $(OLD_NOTES:../%=%)

ifeq ($(EPICS_DEV_SNAPSHOT),)
    NOTES_DOC = ../RELEASE-$(EPICS_SHORT_VERSION).md
endif

NEW_DIR = ../new-notes
NEW_NOTES = $(wildcard $(NEW_DIR)/*.md)

include $(TOP)/configure/RULES

MAKENOTES = ../make-notes.pl
$(COMMON_DIR)/RELEASE_NOTES.md: $(NEW_NOTES) $(NOTES_DOC) $(MAKENOTES)
	@$(RM) $@
	$(PERL) $(MAKENOTES) -o $@ -D -V $(EPICS_SHORT_VERSION) \
	    -d $(abspath $(NEW_DIR)) $(OLD_NOTES)

ifdef NOTES_DOC
$(NOTES_DOC): $(NEW_NOTES) $(MAKENOTES)
	@$(RM) $@
	$(PERL) $(MAKENOTES) -o $@ -V $(EPICS_SHORT_VERSION) \
	    -d $(abspath $(NEW_DIR))
	@echo git add $@
	@echo git rm $(NEW_NOTES)
endif

$(HEADER_MD_FILES): %_h.md: ../HEADER_h.md
	$(EXPAND_TOOL) -t $(INSTALL_LOCATION) -DHEADER=$* $< $@

$(API_RST_FILES): %-api.rst: ../%-API.rst
	@$(RM) $@
	@$(ECHO) Creating $@
	@$(CP) $< $@
	@$(foreach h, $($*_HEADERS), \
	    echo "   $h_h.rst" >> $@;)

ifndef T_A
release doxygen rtd sphinx: inc
	$(MAKE) -C O.$(EPICS_HOST_ARCH) $@
else

release: $(NOTES_DOC)

doxygen: Doxyfile
	@$(MKDIR) $(RTD_SRC)
	$(DOXYGEN)

rtd: doxygen $(API_RST_FILES) $(HEADER_MD_FILES)
	rsync -av $(INSTALL_DOC)/ $(RTD_SRC)/
	rsync -av $(HEADER_MD_FILES) $(RTD_SRC)/
	rsync -av $(API_RST_FILES) $(RTD_SRC)/
	rsync -av ../index.rst ../conf.py $(RTD_SRC)/

sphinx: rtd
	cd $(COMMON_DIR); $(PYTHON) -m sphinx rtd-src readthedocs
	rsync -av $(COMMON_DIR)/readthedocs $(INSTALL_HTML)/
endif

.PHONY: doxygen rtd sphinx
