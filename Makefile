#
# $Id$
#
# Top Level "Diabolical" EPICS Makefile
#        by Matthew Needes and Mike Bordua
#
#  Notes:
#    The build, clean, install, and depends "commands" do not have
#         their own dependency lists; they are instead handled by
#         the build.%, clean.%, etc. dependencies.
#
#    However, the release dependency DOES require the build
#         dependency because the release.% syntax is illegal.

all: build

depends:
	@(for FILE in `/bin/ls config.*.mk 2> /dev/null `;		\
		do							\
			TEMP=`echo $$FILE | cut -d. -f2`;		\
			${MAKE} ${MFLAGS} $@.$$TEMP;			\
		done)

build_libs:
	@(for FILE in `/bin/ls config.*.mk 2> /dev/null `;		\
		do							\
			TEMP=`echo $$FILE | cut -d. -f2`;		\
			${MAKE} ${MFLAGS} $@.$$TEMP;			\
		done)

install_libs:
	@(for FILE in `/bin/ls config.*.mk 2> /dev/null `;		\
		do							\
			TEMP=`echo $$FILE | cut -d. -f2`;		\
			${MAKE} ${MFLAGS} $@.$$TEMP;			\
		done)

build:
	@(for FILE in `/bin/ls config.*.mk 2> /dev/null `;		\
		do							\
			TEMP=`echo $$FILE | cut -d. -f2`;		\
			${MAKE} ${MFLAGS} $@.$$TEMP;			\
		done)

install:
	@(for FILE in `/bin/ls config.*.mk 2> /dev/null `;		\
		do							\
			TEMP=`echo $$FILE | cut -d. -f2`;		\
			${MAKE} ${MFLAGS} $@.$$TEMP;			\
		done)

release: install
	@echo TOP: Building Release...
	@tools/BuildRelease

clean_flags:
	@rm -f dirs.* depends.* build_libs.* \
		install_libs.* build.* install.*

clean: clean_flags
	@echo "TOP: Cleaning"
	@tools/Clean

#  Notes for single architecture build rules:
#    CheckArch only has to be run for dirs.% .  That
#    way it will only be run ONCE when filtering down
#    dependencies.
#
#  CheckArch does not have to be run for cleans
#    because you might want to eliminate binaries for
#    an old architecture.

# DIRS RULE  syntax:  make depends.arch
#               e.g.:    make depends.mv167
#
#  Create dependencies for an architecture.  We MUST
#     do this separately for each architecture because
#     some things may be included on a per architecture 
#     basis.

dirs.%:
	@tools/CheckArch $*
	@echo $*: Creating Directories
	@tools/MakeDirs $*
	@tools/TouchFlag $@ ${TOUCH}

# Depends RULE  syntax:  make depends.arch
#               e.g.:    make depends.mv167
#
#  Create dependencies for an architecture.  We MUST
#     do this separately for each architecture because
#     some things may be included on a per architecture 
#     basis.

depends.%: dirs.%
	@echo $*: Performing Make Depends
	@${MAKE} ${MFLAGS} T_ARCH=$* -f Makefile.subdirs depends
	@tools/TouchFlag $@ ${TOUCH}

# Build_libs RULE  syntax:  make build_libs.arch
#                  e.g.:    make build_libs.arch
#
#  Build libraries  (depends must be finished)

build_libs.%: depends.%
	@echo $*: Building Libraries
	@${MAKE} ${MFLAGS} T_ARCH=$* -f Makefile.subdirs build_libs
	@tools/TouchFlag $@ ${TOUCH}

# Install_libs RULE  syntax:  make install_libs.arch
#                    e.g.:    make install_libs.mv167
#
#  Create dependencies for an architecture.  We MUST
#     do this separately for each architecture because
#     some things may be included on a per architecture 
#     basis.

install_libs.%: build_libs.%
	@echo $*: Installing Libraries
	@${MAKE} ${MFLAGS} T_ARCH=$* -f Makefile.subdirs install_libs
	@tools/TouchFlag $@ ${TOUCH}

# Build RULE  syntax:  make build.arch
#             e.g.:    make build.mv167
#
#  Build executables/libraries for a particular architecture
#
#  Note:
#      Depends must be completed before the build is finished.
#

build.%: install_libs.% 
	@echo $*: Building
	@${MAKE} ${MFLAGS} T_ARCH=$* -f Makefile.subdirs
	@tools/TouchFlag $@ ${TOUCH}

# Install RULE  syntax:  make install.arch
#               e.g.:    make install.mv167
#
#  Install binaries for a particular architecture
#
#  Note:
#      The build must be completed before you can install.
#

install.%: build.%
	@echo $*: Installing
	@${MAKE}	${MFLAGS} T_ARCH=$* -f Makefile.subdirs install
	@tools/TouchFlag $@ ${TOUCH}

# Illegal Syntax

release.%:
	@echo
	@echo "The arch.release syntax is not supported by this build,"
	@echo "   Use 'make release' instead."
	@echo

# Clean RULE  syntax:  make clean.arch
#             e.g.:    make clean.mv167
#
#  Clean files for a particular architecture
#   Erase all build flags
#

clean.%:
	@echo "$*: Cleaning"
	@tools/Clean $*
	@rm -f dirs.$* depends.$* build_libs.$* \
		install_libs.$* build.$* install.$*

