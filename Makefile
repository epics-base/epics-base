
# Top Level "Diabolical" EPICS Makefile
#        by Matthew Needes and Mike Bordua
#
#  Notes:
#    During execution, T_ARCH equals "" when called non-recursively
#                      T_ARCH equals "arch" when called recursively
#
#    The build, clean, install, and depends "commands" do not have
#         their own dependency lists; they are instead handled by
#         the %.build, %.clean, etc. dependencies.
#
#    However, the release dependency DOES require the build
#         dependency because the %.release syntax is illegal.

#   build/clean/install/depends are repeated code, use script ?

# NOTES:
#   Cleans MUST be done in this script and not in Makefile.subdirs

all: build

depends:
	@(for FILE in `/bin/ls config.*.mk 2> /dev/null `;		\
		do							\
			TEMP=`echo $$FILE | cut -d. -f2`;		\
			${MAKE} ${MFLAGS} $$TEMP.depends;	\
		done)

build_libs:
	@(for FILE in `/bin/ls config.*.mk 2> /dev/null `;		\
		do							\
			TEMP=`echo $$FILE | cut -d. -f2`;		\
			${MAKE} ${MFLAGS} $$TEMP.build_libs; \
		done)

install_libs:
	@(for FILE in `/bin/ls config.*.mk 2> /dev/null `;		\
		do							\
			TEMP=`echo $$FILE | cut -d. -f2`;		\
			${MAKE} ${MFLAGS} $$TEMP.install_libs;	\
		done)

build:
	@(for FILE in `/bin/ls config.*.mk 2> /dev/null `;		\
		do							\
			TEMP=`echo $$FILE | cut -d. -f2`;		\
			${MAKE} ${MFLAGS} $$TEMP.build;	\
		done)

install:
	@(for FILE in `/bin/ls config.*.mk 2> /dev/null `;		\
		do							\
			TEMP=`echo $$FILE | cut -d. -f2`;		\
			${MAKE} ${MFLAGS} $$TEMP.install;	\
		done)

release: install
	@echo TOP: Building Release...

clean:
	@tools/Clean

#  Notes for single architecture build rules:
#    Check_Arch only has to be run for %.dirs.  That
#    way it will only be run ONCE when filtering down
#    dependencies.
#
#  Check_Arch does not have to be run for cleans
#    because you might want to eliminate binaries for
#    an old architecture.

# DIRS RULE  syntax:  make arch.depends
#               e.g.:    make mv167.depends
#
#  Create dependencies for an architecture.  We MUST
#     do this separately for each architecture because
#     some things may be included on a per architecture 
#     basis.

%.dirs:
	@tools/Check_Arch $*
	@echo $*: Creating Directories
	@tools/Make_Dirs $*

# Depends RULE  syntax:  make arch.depends
#               e.g.:    make mv167.depends
#
#  Create dependencies for an architecture.  We MUST
#     do this separately for each architecture because
#     some things may be included on a per architecture 
#     basis.

%.depends: %.dirs
	@echo $*: Performing Make Depends
	@${MAKE} ${MFLAGS} T_ARCH=$* -f Makefile.subdirs depends

# Build_libs RULE  syntax:  make arch.build_libs
#                  e.g.:    make mv167.build_libs
#
#  Build libraries  (depends must be finished)

%.build_libs: %.depends
	@echo $*: Building Libraries
	@${MAKE} ${MFLAGS} T_ARCH=$* -f Makefile.subdirs build_libs

# Install_libs RULE  syntax:  make arch.install_libs
#                    e.g.:    make mv167.install_libs
#
#  Create dependencies for an architecture.  We MUST
#     do this separately for each architecture because
#     some things may be included on a per architecture 
#     basis.

%.install_libs: %.build_libs
	@echo $*: Installing Libraries
	@${MAKE} ${MFLAGS} T_ARCH=$* -f Makefile.subdirs install_libs

# Build RULE  syntax:  make arch.build
#             e.g.:    make mv167.build
#
#  Build executables/libraries for a particular architecture
#
#  Note:
#      Depends must be completed before the build is finished.
#

%.build: %.install_libs 
	@echo $*: Building
	@${MAKE} ${MFLAGS} T_ARCH=$* -f Makefile.subdirs

# Install RULE  syntax:  make arch.install
#               e.g.:    make mv167.install
#
#  Install binaries for a particular architecture
#
#  Note:
#      The build must be completed before you can install.
#

%.install: %.build
	@echo $*: Installing
	@${MAKE}	${MFLAGS} T_ARCH=$* -f Makefile.subdirs install

# Illegal Syntax

%.release:
	@echo
	@echo "The arch.release syntax is not supported by this build,"
	@echo "   Use 'make release' instead."
	@echo

# Clean RULE  syntax:  make arch.clean
#             e.g.:    make mv167.clean
#
#  Clean files for a particular architecture
#

%.clean:
	@echo "$*: Cleaning"
	@tools/Clean $*

