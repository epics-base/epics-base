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
DOCS += ReleaseChecklist.md
DOCS += gdb.md

OLD_NOTES = $(wildcard ../RELEASE-*.md)
DOCS += $(OLD_NOTES:../%=%)

NOTES_FILE = RELEASE-$(EPICS_SHORT_VERSION).md
# NOTES_PATH works in both source and build directories
ifdef T_A
  NOTES_PATH = ../$(NOTES_FILE)
else
  NOTES_PATH = $(NOTES_FILE)
endif

ifneq ($(EPICS_DEV_SNAPSHOT),)
  # Between releases, our special release targets are disabled
  REASON = EPICS_DEV_SNAPSHOT not empty
  NOTES_PATH =
else
  # Not a snapshot. User could be creating a new release,
  # or this is a build of an already-tagged release.

  # Are we in the process of releasing?
  NOTES_STAT := $(and $(wildcard $(TOP)/.git), $(shell which git), \
      $(firstword $(shell git status --porcelain $(NOTES_PATH))))
  ifeq ($(NOTES_STAT),A)
    # New file was added to the Git index; allow unrelease
  else ifeq ($(NOTES_STAT),AD)
    # File was added to the Git index but deleted; allow unrelease
  else ifeq ($(wildcard $(NOTES_PATH)),)
    # No file yet, "make release" creates it
  else ifeq ($(NOTES_STAT),)
    # File in repo, unchanged
    REASON := file $(NOTES_FILE) exists
    NOTES_PATH =
  endif
endif

include $(TOP)/configure/RULES

help:
	@echo "Makefile targets for documentation:"
	@echo "    all       - install some files (default)"
	@echo "    help      - Print this list of make targets"
	@echo "    release   - Create RELEASE-<ver>.md, add to git"
	@echo "    unrelease - Undo 'make release'"
	@echo "    doxygen   - Generate Doxygen HTML output"
	@echo "    rtd       - Install files used by read-the-docs"
	@echo "    sphinx    - Run Sphinx for all release docs"

ifndef T_A
  # Delegate these targets to the O.$(T_A) build directory
  release unrelease doxygen rtd sphinx: inc
	$(MAKE) -C O.$(EPICS_HOST_ARCH) $@

else
  # Special release target rules
  MAKENOTES = ../make-notes.pl
  NEW_DIR = ../new-notes
  NEW_NOTES = $(wildcard $(NEW_DIR)/*.md)

  ifneq ($(NOTES_PATH),)
    $(NOTES_PATH): $(NEW_NOTES) $(MAKENOTES)
	@$(RM) $@
	$(PERL) $(MAKENOTES) -o $@ -V $(EPICS_SHORT_VERSION) \
	    -d $(abspath $(NEW_DIR)) $(OLD_NOTES)
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

    REL_DEPS = release-git $(NOTES_PATH)
  else
    DEV_FLAG = -D
    release unrelease:
	$(error "make $@" not available, $(REASON))
  endif

  # Rules to create RELEASE_NOTES.md
  ifeq ($(wildcard ../$(NOTES_FILE)),)
    # No RELEASE-<ver>.md file, add new-notes to RELEASE_NOTES.md
    $(COMMON_DIR)/RELEASE_NOTES.md: $(REL_DEPS) $(NEW_NOTES) $(MAKENOTES)
	@$(RM) $@
	$(PERL) $(MAKENOTES) -o $@ -V $(EPICS_SHORT_VERSION) $(DEV_FLAG) \
	    -d $(abspath $(NEW_DIR)) $(OLD_NOTES) $(NOTES_PATH)
  else
    # RELEASE-<ver>.md exists, we're building a tagged version
    $(COMMON_DIR)/RELEASE_NOTES.md: ../$(NOTES_FILE)
	@$(RM) $@
	$(PERL) $(MAKENOTES) -o $@ -V $(EPICS_SHORT_VERSION) $(OLD_NOTES)
  endif

  $(HEADER_MD_FILES): %_h.md: ../HEADER_h.md
	$(EXPAND_TOOL) -t $(INSTALL_LOCATION) -DHEADER=$* $< $@

  $(API_RST_FILES): %-api.rst: ../%-API.rst
	@$(RM) $@
	@$(ECHO) Creating $@
	@$(CP) $< $@
	@$(foreach h, $($*_HEADERS), \
	    echo "   $h_h.rst" >> $@;)

  DOX = doxygen
  doxygen: Doxyfile
	@$(MKDIR) $(RTD_SRC)
	$(DOXYGEN)

  # Use "make sphinx DOX=" to skip running doxygen
  RTD = rtd
  rtd: $(DOX) $(API_RST_FILES) $(HEADER_MD_FILES)
	rsync -av --exclude=RELEASE-*.md $(INSTALL_DOC)/ $(RTD_SRC)/
	rsync -av $(HEADER_MD_FILES) $(RTD_SRC)/
	rsync -av $(API_RST_FILES) $(RTD_SRC)/
	rsync -av ../index.rst ../conf.py $(RTD_SRC)/

  # Use "make sphinx RTD=" to skip earlier steps
  sphinx: $(RTD)
	cd $(COMMON_DIR); $(PYTHON) -m sphinx rtd-src readthedocs
	rsync -av $(COMMON_DIR)/readthedocs $(INSTALL_HTML)/
endif

.PHONY: release help release-git unrelease doxygen rtd sphinx
.NOTPARALLEL: release unrelease doxygen rtd sphinx
