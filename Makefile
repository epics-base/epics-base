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

all: install

build:
	@(for ARCH in ${BUILD_ARCHS}; do    \
		${MAKE} $@.$${ARCH}; \
	done)

install:
	@(for ARCH in ${BUILD_ARCHS}; do    \
		${MAKE} $@.$${ARCH}; \
	done)

depends:
	@(for ARCH in ${BUILD_ARCHS}; do    \
		${MAKE} $@.$${ARCH}; \
	done)

clean:
	@(for ARCH in ${BUILD_ARCHS}; do    \
		${MAKE} $@.$${ARCH}; \
	done)

uninstall:
	@(for ARCH in ${BUILD_ARCHS}; do    \
		${MAKE} $@.$${ARCH}; \
	done)

release: 
	@echo TOP: Creating Release...
	@./MakeRelease ${TOP}

built_release: install
	@echo TOP: Creating Fully Built Release...
	@./MakeRelease ${TOP} -b

#  Notes for single architecture build rules:
#    CheckArch only has to be run for dirs.% .  That
#    way it will only be run ONCE when filtering down
#    dependencies.
#
#  CheckArch does not have to be run for cleans
#    because you might want to eliminate binaries for
#    an old architecture.

# DIRS RULE  syntax:  make depends.arch
#              e.g.:  make depends.mv167
#
#  Create dependencies for an architecture.  We MUST
#     do this separately for each architecture because
#     some things may be included on a per architecture 
#     basis.

build.%:
	@echo $*: Building
	@${MAKE} ${MFLAGS} T_A=$* -f Makefile.subdirs build

install.%:
	@echo $*: Installing
	@${MAKE} ${MFLAGS} T_A=$* -f Makefile.subdirs install

depends.%:
	@echo $*: Performing Make Depends
	@${MAKE} ${MFLAGS} T_A=$* -f Makefile.subdirs depends

# Illegal Syntax

release.%:
	@echo
	@echo "The release.arch syntax is not supported by this build."
	@echo "   Use 'make release' or 'make built_release' instead."
	@echo

uninstall.%:
	@echo "TOP: Uninstalling $* "
	@rm -rf $(INSTALL_LOCATION_BIN)/$* $(INSTALL_LOCATION_LIB)/$* \
		$(INSTALL_LOCATION)/dbd $(INSTALL_MAN) $(INSTALL_INCLUDE) 
	@rm -rf  rec.bak rec

clean.%:
	@echo "TOP: Cleaning $* "
	@find src -type d -name "O.$*" -prune -exec rm -rf {} \;
	@find config -type d -name "O.$*" -prune -exec rm -rf {} \;



