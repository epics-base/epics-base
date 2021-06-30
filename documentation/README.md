# Installation Instructions {#install}

## EPICS Base Release 7.0.5

-----

### Table of Contents

  - [What is EPICS base?](#what-is-epics-base?)
  - [What is new in this release?](#what-is-new-in-this-release?)
  - [Copyright](#copyright)
  - [Supported platforms](#supported-platforms)
  - [Supported compilers](#supported-compilers)
  - [Software requirements](#software-requirements)
  - [Host system storage requirements](#host-system-storage-requirements)
  - [Documentation](#documentation)
  - [Directory Structure](#directory-structure)
  - [Site-specific build configuration](#site-specific-build-configuration)
  - [Building EPICS base](#building-epics-base)
  - [Example application and extension](#example-application-and-extension)
  - [Multiple host platforms](#multiple-host-platforms)

-----

### What is EPICS base?

The Experimental Physics and Industrial Control Systems (EPICS) is an
extensible set of software components and tools with which application
developers can create a control system. This control system can be
used to control accelerators, detectors, telescopes, or other
scientific experimental equipment. EPICS base is the set of core
software, i.e. the components of EPICS without which EPICS would not
function. EPICS base allows an arbitrary number of target systems,
IOCs (input/output controllers), and host systems, OPIs (operator
interfaces) of various types.

### What is new in this release?

Please check the `documentation/RELEASE_NOTES.md` file for
description of changes and release migration details.

### Copyright

Please review the `LICENSE` file included in the distribution for
legal terms of usage.

### Supported platforms

The list of platforms supported by this version of EPICS base is given
in the `configure/CONFIG_SITE` file. If you are trying to build EPICS
Base on an unlisted host or for a different target machine you must
have the proper host/target cross compiler and header files, and you
will have to create and add the appropriate new configure files to the
base/configure/os/directory. You can start by copying existing
configuration files in the configure/os directory and then make
changes for your new platforms.

### Supported compilers

This version of EPICS base has been built and tested using the host
vendor's C and C++ compilers, as well as the GNU gcc and g++
compilers. The GNU cross-compilers work for all cross-compiled
targets. You may need the C and C++ compilers to be in your search
path to do EPICS builds; check the definitions of CC and CCC in
`base/configure/os/CONFIG.<host>.<host>` if you have problems.

### Software requirements

#### GNU make

You must use the GNU version of `make` for EPICS builds. Set your path
so that version 4.1 or later is available. The macOS version of `make`
is older but does still work.

#### Perl

You must have Perl version 5.10 or later installed. The EPICS
configuration files do not specify the perl full pathname, so the perl
executable must be found through your normal search path.

#### Unzip and tar (Winzip on WIN32 systems)

You must have tools available to unzip and untar the EPICS base
distribution file.

#### Target systems

EPICS supports IOCs running on embedded platforms such as VxWorks and
RTEMS built using a cross-compiler, and also supports soft IOCs
running as processes on the host platform.

#### vxWorks

You must have vxWorks 6.8 or later installed if any of your target
systems are vxWorks systems; the C++ compiler from older versions cannot
compile recently developed code. The vxWorks installation provides the
cross-compiler and header files needed to build for these targets. The
absolute path to and the version number of the vxWorks installation
must be set in the `base/configure/os/CONFIG_SITE.Common.vxWorksCommon`
file or in one of its target-specific overrides.

Consult the [vxWorks 6.x](https://epics.anl.gov/base/vxWorks6.php) EPICS
web pages about and the vxWorks documentation for information about
configuring your vxWorks operating system for use with EPICS.

#### RTEMS

For RTEMS targets, you need RTEMS core and toolset version 4.9.x or
4.10.x. RTEMS 5 is experimental in EPICS 7.0.6.

#### Command Line Editing

GNU readline and other similar libraries can be used by the IOC shell
to provide command line editing and command line history recall. The
GNU readline development package (or Apple's emulator on macOS) must
be installed for a target when its build configuration variable
`COMMANDLINE_LIBRARY` is set to `READLINE`. The default specified in
`CONFIG_COMMON` is `EPICS`, but most linux target builds can detect if
readline is available and will then use it. RTEMS targets may be
configured to use `LIBTECLA` if available, and on vxWorks the OS's
ledLib line-editing library is normally used.

### Host system storage requirements

The compressed tar file is approximately 3 MB in size. The
distribution source tree takes up approximately 21 MB. A 64-bit host
architecture may need around 610 MB to compile, while cross-compiled
targets are somewhat smaller.

### Documentation

EPICS documentation is available through the [EPICS website](https://epics.anl.gov/) at Argonne.

Release specific documentation can also be found in the
`base/documentation` directory of the distribution.

### Directory Structure

#### Distribution directory structure

```
    base                Root directory of the distribution
    base/configure      Build rules and OS-independent config files
    base/configure/os   OS-dependent build config files
    base/documentation  Distribution documentation
    base/src            Source code in various subdirectories
    base/startup        Scripts for setting up path and environment
```

#### Directories created by the build

These are created in the root directory of the installation (`base`
above) or under the directory pointed to by the `INSTALL_LOCATION`
configuration variable if that has been set.

```
    bin               Installed scripts and executables in subdirs
    cfg               Installed build configuration files
    db                Installed database files
    dbd               Installed database definition files
    html              Installed html documentation
    include           Installed header files
    include/os        Installed OS-specific header files in subdirs
    include/compiler  Installed compiler-specific header files
    lib               Installed libraries in arch subdirectories
    lib/perl          Installed perl modules
    templates         Installed templates
```

#### `base/documentation` Directory

This contains documents on how to setup, build, and install EPICS.

```
    README.md           This file
    RELEASE_NOTES.md    Notes on release changes
    KnownProblems.html  List of known problems and workarounds
```

#### `base/startup` Directory

This contains several example scripts that show how to set up the
build environment and PATH for using EPICS. Sites would usually copy and/or modify these files as appropriate for their environment; they are not used by the build system at all.

```
    EpicsHostArch  Shell script to set EPICS_HOST_ARCH env variable
    unix.csh       C shell script to set path and env variables
    unix.sh        Bourne shell script to set path and env variables
    win32.bat      Bat file example to configure win32-x86 target
    windows.bat    Bat file example to configure windows-x64 target
```

#### `base/configure` directory

This contains build-system files providing definitions and rules
required by GNU Make to build EPICS. Users should only need to modify the `CONFIG_SITE` files to configure the EPICS build.

```
    CONFIG               Main entry point for building EPICS
    CONFIG.CrossCommon   Cross build definitions
    CONFIG.gnuCommon     Gnu compiler build definitions for all archs
    CONFIG_ADDONS        Definitions for <osclass> and DEFAULT options
    CONFIG_APP_INCLUDE
    CONFIG_BASE          EPICS base tool and location definitions
    CONFIG_BASE_VERSION  Definitions for EPICS base version number
    CONFIG_COMMON        Definitions common to all builds
    CONFIG_ENV           Definitions of EPICS environment variables
    CONFIG_FILE_TYPE
    CONFIG_SITE          Site specific make definitions
    CONFIG_SITE_ENV      Site defaults for EPICS environment variables
    MAKEFILE             Installs CONFIG* RULES* creates
    RELEASE              Location of external products
    RULES                Includes appropriate rules file
    RULES.Db             Rules for database and database definition files
    RULES.ioc            Rules for application iocBoot/ioc* directory
    RULES_ARCHS          Definitions and rules for building architectures
    RULES_BUILD          Build and install rules and definitions
    RULES_DIRS           Definitions and rules for building subdirectories
    RULES_EXPAND
    RULES_FILE_TYPE
    RULES_TARGET
    RULES_TOP            Rules specific to a <top> dir only
    Sample.Makefile      Sample makefile with comments
```

#### `base/configure/os` Directory

Files in here provide definitions that are shared by or specific to particular host and/or target architectures. Users should only need to modify the `CONFIG_SITE` files in this directory to configure the EPICS build.

```
    CONFIG.<host>.<target>            Definitions for a specific host-target combination
    CONFIG.Common.<target>            Definitions for a specific target, any host
    CONFIG.<host>.Common              Definitions for a specific host, any target
    CONFIG.UnixCommon.Common          Definitions for Unix hosts, any target
    CONFIG.Common.UnixCommon          Definitions for Unix targets, any host
    CONFIG.Common.RTEMS               Definitions for all RTEMS targets, any host
    CONFIG.Common.vxWorksCommon       Definitions for all vxWorks targets, any host
    CONFIG_SITE.<host>.<target>       Local settings for a specific host-target combination
    CONFIG_SITE.Common.<target>       Local settings for a specific target, any host
    CONFIG_SITE.<host>.Common         Local settings for a specific host, any target
    CONFIG_SITE.Common.RTEMS          Local settings for all RTEMS targets, any host
    CONFIG_SITE.Common.vxWorksCommon  Local settings for all vxWorks targets, any host
```

### Building EPICS base

#### Unpack file

Unzip and untar the distribution file. Use WinZip on Windows
systems.

#### Set environment variables

Files in the base/startup directory have been provided to help set
required path and other environment variables.

* **`EPICS_HOST_ARCH`**

Some host builds of EPICS require that the environment variable
`EPICS_HOST_ARCH` be defined. The perl script `EpicsHostArch.pl` in the
`base/startup` directory prints the value which the build will use if
the variable is not set before the build starts. Architecture names
start with the operating system followed by a dash and the host CPU
architecture, e.g. `linux-x86_64`. Some architecture names have another
dash followed by another keyword, for example when building for Windows
but using the MinGW compiler the name must be `windows-x64-mingw`. See
`configure/CONFIG_SITE` for a list of supported host architecture names.

* **`PATH`**
As already mentioned, you must have the `perl` executable and you may
need C and C++ compilers in your search path. When building base you
must have `echo` in your search path. For Unix host builds you will
also need `cp`, `rm`, `mv`, and `mkdir` in your search path. Some Unix
systems may also need `ar` and `ranlib`, and the C/C++ compilers may
require `as` and `ld` in your path. On Solaris systems you need
`uname` in your path.

* **`LD_LIBRARY_PATH`**
EPICS shared libraries and executables normally contain the full path
to any libraries they require, so setting this variable is not usually
necessary. However, if you move the EPICS installation to a new
location after building it then in order for the shared libraries to
be found at runtime it may need to be set, or some equivalent
OS-specific mechanism such as `/etc/ld.so.conf` on Linux must be used.
Shared libraries are now built by default on all Unix type hosts.

### Site-specific build configuration

#### Site configuration

To configure EPICS, you may want to modify some values set in the
following files:
>>>>>>> mirror/3.15

```
    configure/CONFIG_SITE      Build settings. Specify target archs.
    configure/CONFIG_SITE_ENV  Environment variable defaults
```

#### Host configuration

To configure each host system, you can override the default
definitions by adding a new settings file (or editing an existing
settings file) in the `configure/os` directory with your override
definitions. The settings file has the same name as the definitions
file to be overridden except with `CONFIG` in the name changed to
`CONFIG_SITE`.

```
    configure/os/CONFIG.<host>.<host>       Host self-build definitions
    configure/os/CONFIG.<host>.Common       Host common build definitions
    configure/os/CONFIG_SITE.<host>.<host>  Host self-build overrides
    configure/os/CONFIG_SITE.<host>.Common  Host common build overrides
```

#### Target configuration

To configure each target system, you may override the default
definitions by adding a new settings file (or editing an existing
settings file) in the `configure/os` directory with your override
definitions. The settings file has the same name as the definitions
file to be overridden except with `CONFIG` in the name changed to
`CONFIG_SITE`.

```
    configure/os/CONFIG.Common.<target>       Target common definitions
    configure/os/CONFIG.<host>.<target>       Host-target definitions
    configure/os/CONFIG_SITE.Common.<target>  Target common overrides
    configure/os/CONFIG_SITE.<host>.<target>  Host-target overrides
```

#### Build EPICS base

After configuring the build you should be able to build EPICS base
by issuing the following commands in the distribution's root
directory (base):

```
    make distclean
    make
```

The command `make distclean` will remove all files and
directories generated by a previous build. The command `make`
will build and install everything for the configured host and
targets.

It is recommended that you do a `make distclean` at the
root directory of an EPICS directory structure before each complete
rebuild to ensure that all components will be rebuilt.

In some cases GNU Make may have been installed as `gmake` or
`gnumake`, in which case the above commands will have to be adjusted
to match.

### Example application and extension

A perl tool `makeBaseApp.pl` and several template applications are
included in the distribution. This script instantiates the selected
template into an empty directory to provide an example application
that can be built and then executed to try out this release of base.

Instructions for building and executing the EPICS example application
can be found in the section "Example Application" of Chapter 2,
"Getting Started", in the "EPICS Application Developer's Guide". 
The "Example IOC Application" section briefly explains how to
create and build an example application in a user created &lt;top>
directory. It also explains how to run the example application on a
vxWorks ioc or as a process on the host system. By running the example
application as a host-based IOC, you will be able to quickly implement
a complete EPICS system and be able to run channel access clients on
the host system.

Another perl script `makeBaseExt.pl` is also included in the
distribution file for creating an extensions tree and sample
application that can also be built and executed. Both these scripts
are installed into the install location `bin/<hostarch>` directory
during the base build.

### Multiple host platforms

You can build using a single EPICS directory structure on multiple
host systems and for multiple cross target systems. The intermediate
and binary files generated by the build will be created in separate
subdirectories and installed into the appropriate separate host/target
install directories.

EPICS executables and perl scripts are installed into the
`$(INSTALL_LOCATION)/bin/<arch>` directories. Libraries are installed
into $`(INSTALL_LOCATION)/lib/<arch>`. The default definition for
`$(INSTALL_LOCATION)` is `$(TOP)` which is the root directory in the
distribution directory structure, `base`. Intermediate object files
are stored in `O.<arch>` source subdirectories during the build
process, to allow objects for multiple cross target architectures
to be maintained at the same time.

To build EPICS base for a specific
host/target combination you must have the proper host/target C/C++
cross compiler and target header files and the base/configure/os
directory must have the appropriate configure files.
