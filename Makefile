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
# Revision 1.13  1994/09/07  19:15:17  jba
# Modified to eork with extensions and do depends
#
# Revision 1.12  1994/08/21  00:55:51  mcn
# New stuff.
#
# Revision 1.11  1994/08/19  15:38:01  mcn
# Dependencies are now generated with a "make release".
#
# Revision 1.10  1994/08/12  18:51:29  mcn
# Added Log and/or Id.
#
#

EPICS=..
include $(EPICS)/config/CONFIG_SITE

T_A = ${BUILD_ARCHS}

all: install

pre_build:
	@(for ARCH in ${T_A};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

build_libs:
	@(for ARCH in ${T_A};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

install_libs:
	@(for ARCH in ${T_A};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

build_prod:
	@(for ARCH in ${T_A};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

install_prod:
	@(for ARCH in ${T_A};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

install_man:
	@(for ARCH in ${T_A};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

install_includes:
	@(for ARCH in ${T_A};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

build:
	@(for ARCH in ${T_A};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

install:
	@(for ARCH in ${T_A};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

depends:
	@(for ARCH in ${T_A};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

clean:
	@(for ARCH in ${T_A};					\
		do							\
			${MAKE} ${MFLAGS} $@.$$ARCH;			\
		done)

release: depends install
	@echo TOP: Creating Release...
	@tools/MakeRelease

built_release: depends install
	@echo TOP: Creating Fully Built Release...
	@tools/MakeRelease -b

#clean:
#	@echo "TOP: Cleaning"
#	@tools/Clean

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

pre_build.%: dirs.%
	@echo $*: Performing Pre Build
	@${MAKE} ${MFLAGS} ARCH=$* -f Makefile.subdirs pre_build


build_libs.%: dirs.%
	@echo $*: Building Libraries
	@${MAKE} ${MFLAGS} ARCH=$* -f Makefile.subdirs build_libs


install_libs.%: dirs.%
	@echo $*: Installing Libraries
	@${MAKE} ${MFLAGS} ARCH=$* -f Makefile.subdirs install_libs

build_prod.%: dirs.% 
	@echo $*: Building
	@${MAKE} ${MFLAGS} ARCH=$* -f Makefile.subdirs build_prod

install_prod.%: dirs.% 
	@echo $*: Building
	@${MAKE} ${MFLAGS} ARCH=$* -f Makefile.subdirs install_prod

install_man.%: dirs.% 
	@echo $*: Building
	@${MAKE} ${MFLAGS} ARCH=$* -f Makefile.subdirs install_man

install_includes.%: dirs.% 
	@echo $*: Building
	@${MAKE} ${MFLAGS} ARCH=$* -f Makefile.subdirs install_includes

build.%: dirs.% 
	@echo $*: Building
	@${MAKE} ${MFLAGS} ARCH=$* -f Makefile.subdirs build

install.%: dirs.%
	@echo $*: Installing
	@${MAKE} ${MFLAGS} ARCH=$* -f Makefile.subdirs install

depends.%: dirs.%
	@echo $*: Performing Make Depends
	@${MAKE} ${MFLAGS} ARCH=$* -f Makefile.subdirs depends

clean.%: dirs.%
	@echo $*: Cleaning
	@${MAKE} ${MFLAGS} ARCH=$* -f Makefile.subdirs clean

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

#clean.%:
#	@echo "$*: Cleaning"
#	@tools/Clean $*

