#
# $Id$
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
# $Log$
# Revision 1.11  1994/08/19  15:38:01  mcn
# Dependencies are now generated with a "make release".
#
# Revision 1.10  1994/08/12  18:51:29  mcn
# Added Log and/or Id.
#
#

EPICS=..
include $(EPICS)/config/CONFIG_SITE

all: install

pre_build:
	@(for ARCH in ${BUILD_ARCHS};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

build_libs:
	@(for ARCH in ${BUILD_ARCHS};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

install_libs:
	@(for ARCH in ${BUILD_ARCHS};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

build:
	@(for ARCH in ${BUILD_ARCHS};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

#  For installs, SDR utilities must be built if the current
#	architecture is the host architecture.

install:
	@(for ARCH in ${BUILD_ARCHS};					\
		do							\
			if [ "$$ARCH" = "${HOST_ARCH}" ];		\
			then						\
				${MAKE} ${MFLAGS} sdr.$$ARCH;		\
			else						\
				${MAKE} ${MFLAGS} $@.$$ARCH;		\
			fi						\
		done)

sdr: sdr.${HOST_ARCH}

depends:
	@(for ARCH in ${BUILD_ARCHS};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

release: depends install
	@echo TOP: Creating Release...
	@tools/MakeRelease

built_release: depends install
	@echo TOP: Creating Fully Built Release...
	@tools/MakeRelease -b

clean:
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
#              e.g.:  make depends.mv167
#
#  Create dependencies for an architecture.  We MUST
#     do this separately for each architecture because
#     some things may be included on a per architecture 
#     basis.

dirs.%:
	@tools/CheckArch $*
	@echo $*: Creating Directories
	@tools/MakeDirs $*

# Pre_build RULE  syntax:  make pre_build.arch
#                  e.g.:   make pre_build.arch
#
#  Build libraries  (depends must be finished)

pre_build.%: dirs.%
	@echo $*: Performing Pre Build
	@${MAKE} ${MFLAGS} T_A=$* -f Makefile.subdirs pre_build

# Build_libs RULE  syntax:  make build_libs.arch
#                  e.g.:    make build_libs.arch
#
#  Build libraries  (depends must be finished)

build_libs.%: pre_build.%
	@echo $*: Building Libraries
	@${MAKE} ${MFLAGS} T_A=$* -f Makefile.subdirs build_libs

# Install_libs RULE  syntax:  make install_libs.arch
#                    e.g.:    make install_libs.mv167
#
#  Create dependencies for an architecture.  We MUST
#     do this separately for each architecture because
#     some things may be included on a per architecture 
#     basis.

install_libs.%: build_libs.%
	@echo $*: Installing Libraries
	@${MAKE} ${MFLAGS} T_A=$* -f Makefile.subdirs install_libs

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
	@${MAKE} ${MFLAGS} T_A=$* -f Makefile.subdirs

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
	@${MAKE} ${MFLAGS} T_A=$* -f Makefile.subdirs install

# SDR RULES 	syntax:  make sdr.arch
#		e.g.:    make sdr.sun4
#
# Builds headers for SDR utilities.  sdr_no_install does not
#	make sure the objects for a particular arch are
#	installed before it runs the SDR utilities.

sdr.%: install.%
	@echo $*: Making SDR;						\
	cd rec;								\
	${MAKE} ${MFLAGS} T_A=$* -f Makefile.Unix

sdr_no_install.%:
	@echo $*: Making SDR;						\
	cd rec;								\
	${MAKE} ${MFLAGS} T_A=$* -f Makefile.Unix
	
# Depends RULE  syntax:  make depends.arch
#               e.g.:    make depends.mv167
#
#  Create dependencies for an architecture.  We MUST
#     do this separately for each architecture because
#     some things may be included on a per architecture 
#     basis.
#
#  This dependency must be invoked manually, it is
#     not automatic in the build.  However, since depends
#     are precomputed, this does not have to be done by
#     most sites.

depends.%: dirs.%
	@echo $*: Performing Make Depends
	@${MAKE} ${MFLAGS} T_A=$* -f Makefile.subdirs depends

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
#

clean.%:
	@echo "$*: Cleaning"
	@tools/Clean $*

