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

OLD_NOTES = $(wildcard ../RELEASE-*.md)
DOCS += $(OLD_NOTES:../%=%)

ifneq ($(EPICS_DEV_SNAPSHOT),)
  # Between releases the release target is disabled
  DOCS += RELEASE_NOTES.md
  REASON = EPICS_DEV_SNAPSHOT not empty
else
  # Not a snapshot, user may be creating a new release
  ifdef T_A
    DOC = ../
  endif
  # Could we be in the middle of anything?
  NOTES_FILE = RELEASE-$(EPICS_SHORT_VERSION).md
  NOTES_PATH = $(DOC)$(NOTES_FILE)
  NOTES_GIT := $(firstword $(shell git status --porcelain $(NOTES_PATH)))
  ifeq ($(NOTES_GIT),A)
    # New file was added to the Git index; allow unrelease
  else ifeq ($(NOTES_GIT),AD)
    # File was added to the Git index but deleted; allow unrelease
  else ifeq ($(wildcard $(NOTES_PATH)),)
    # No file yet, allow release
  else
    # File exists but isn't in Git
    REASON := $(NOTES_FILE) exists but isn't in Git
    NOTES_FILE =
    NOTES_PATH =
  endif
endif

NEW_DIR = ../new-notes
NEW_NOTES = $(wildcard $(NEW_DIR)/*.md)

include $(TOP)/configure/RULES

ifndef T_A
  release unrelease doxygen rtd sphinx: inc
	$(MAKE) -C O.$(EPICS_HOST_ARCH) $@
else

MAKENOTES = ../make-notes.pl

ifneq ($(NOTES_PATH),)
  $(NOTES_PATH): $(NEW_NOTES) $(MAKENOTES)
	@$(RM) $@
	$(PERL) $(MAKENOTES) -o $@ -V $(EPICS_SHORT_VERSION) \
	    -d $(abspath $(NEW_DIR))
  release-git: $(NOTES_PATH)
	$(if $(NEW_NOTES), \
	    git rm -q $(NEW_NOTES))
	git add $<
  release: $(INSTALL_DOC)/RELEASE_NOTES.md $(INSTALL_DOC)/$(NOTES_FILE)

  unrelease:
	$(if $(wildcard $(NOTES_PATH)), \
	    git restore --staged $(NOTES_PATH); \
	    $(RM) $(NOTES_PATH))
	git restore --staged $(NEW_DIR)
	git restore $(NEW_DIR)

  REL_DEP = release-git $(NOTES_PATH)
else
  REL_DEV = -D
  release unrelease:
	$(error "make $@" not available, $(REASON))
endif

$(COMMON_DIR)/RELEASE_NOTES.md: $(REL_DEP) $(NEW_NOTES) $(MAKENOTES)
	@$(RM) $@
	$(PERL) $(MAKENOTES) -o $@ -V $(EPICS_SHORT_VERSION) $(REL_DEV) \
	    -d $(abspath $(NEW_DIR)) $(OLD_NOTES) $(NOTES_PATH)

$(HEADER_MD_FILES): %_h.md: ../HEADER_h.md
	$(EXPAND_TOOL) -t $(INSTALL_LOCATION) -DHEADER=$* $< $@

$(API_RST_FILES): %-api.rst: ../%-API.rst
	@$(RM) $@
	@$(ECHO) Creating $@
	@$(CP) $< $@
	@$(foreach h, $($*_HEADERS), \
	    echo "   $h_h.rst" >> $@;)

doxygen: Doxyfile
	@$(MKDIR) $(RTD_SRC)
	$(DOXYGEN)
DOX = doxygen

# Use "make sphinx DOX=" to skip running doxygen
rtd: $(DOX) $(API_RST_FILES) $(HEADER_MD_FILES)
	rsync -av --exclude=RELEASE-*.md $(INSTALL_DOC)/ $(RTD_SRC)/
	rsync -av $(HEADER_MD_FILES) $(RTD_SRC)/
	rsync -av $(API_RST_FILES) $(RTD_SRC)/
	rsync -av ../index.rst ../conf.py $(RTD_SRC)/
RTD = rtd

# Use "make sphinx RTD=" to skip earlier steps
sphinx: $(RTD)
	cd $(COMMON_DIR); $(PYTHON) -m sphinx rtd-src readthedocs
	rsync -av $(COMMON_DIR)/readthedocs $(INSTALL_HTML)/
endif

.PHONY: release release-git unrelease doxygen rtd sphinx
