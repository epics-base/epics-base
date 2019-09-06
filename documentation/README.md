# Installation Instructions

## EPICS Base Release 3.15.6

  

-----

### Table of Contents

  - [What is EPICS base?](#0_0_1)
  - [What is new in this release?](#0_0_2)
  - [Copyright](#0_0_3)
  - [Supported platforms](#0_0_4)
  - [Supported compilers](#0_0_5)
  - [Software requirements](#0_0_6)
  - [Host system storage requirements](#0_0_7)
  - [Documentation](#0_0_8)
  - [Directory Structure](#0_0_10)
  - [Build related components](#0_0_11)
  - [Building EPICS base (Unix and Win32)](#0_0_12)
  - [Example application and extension](#0_0_13)
  - [Multiple host platforms](#0_0_14)

-----

### <span id="0_0_1">What is EPICS base?</span>

The Experimental Physics and Industrial Control Systems (EPICS) is an
extensible set of software components and tools with which application
developers can create a control system. This control system can be
used to control accelerators, detectors, telescopes, or other
scientific experimental equipment. EPICS base is the set of core
software, i.e. the components of EPICS without which EPICS would not
function. EPICS base allows an arbitrary number of target systems,
IOCs (input/output controllers), and host systems, OPIs (operator
interfaces) of various types.

### <span id="0_0_2">What is new in this release?</span>

Please check the RELEASE\_NOTES file in the distribution for
description of changes and release migration details.

### <span id="0_0_3">Copyright</span>

Please review the LICENSE file included in the distribution for legal
terms of usage.

### <span id="0_0_4">Supported platforms</span>

The list of platforms supported by this version of EPICS base is given
in the configure/CONFIG\_SITE file. If you are trying to build EPICS
Base on an unlisted host or for a different target machine you must
have the proper host/target cross compiler and header files, and you
will have to create and add the appropriate new configure files to the
base/configure/os/directory. You can start by copying existing
configuration files in the configure/os directory and then make
changes for your new platforms.

### <span id="0_0_5">Supported compilers</span>

This version of EPICS base has been built and tested using the host
vendor's C and C++ compilers, as well as the GNU gcc and g++
compilers. The GNU cross-compilers work for all cross-compiled
targets. You may need the C and C++ compilers to be in your search
path to do EPICS builds; check the definitions of CC and CCC in
base/configure/os/CONFIG.&lt;host>.&lt;host> if you have problems.

### <span id="0_0_6">Software requirements</span>

**GNU make**  
You must use GNU make, gnumake, for any EPICS builds. Set your path so
that a gnumake version 3.81 or later is available.

**Perl**  
You must have Perl version 5.8.1 or later installed. The EPICS
configuration files do not specify the perl full pathname, so the perl
executable must be found through your normal search path.

**Unzip and tar (Winzip on WIN32 systems)**  
You must have tools available to unzip and untar the EPICS base
distribution file.

**Target systems**  
EPICS supports IOCs running on embedded platforms such as VxWorks and
RTEMS built using a cross-compiler, and also supports soft IOCs
running as processes on the host platform.

**vxWorks**  
You must have vxWorks 5.5.x or 6.x installed if any of your target
systems are vxWorks systems; the C++ compiler for vxWorks 5.4 is now
too old to support. The vxWorks installation provides the
cross-compiler and header files needed to build for these targets. The
absolute path to and the version number of the vxWorks installation
must be set in the base/configure/os/CONFIG\_SITE.Common.vxWorksCommon
file or in one of its target-specific overrides.

Consult the [vxWorks 5.x](https://epics.anl.gov/base/tornado.php) or
[vxWorks 6.x](https://epics.anl.gov/base/vxWorks6.php) EPICS web pages
about and the vxWorks documentation for information about configuring
your vxWorks operating system for use with EPICS.

**RTEMS**  
For RTEMS targets, you need RTEMS core and toolset version 4.9.2 or
later.

**GNU readline or Tecla library**  
GNU readline and Tecla libraries can be used by the IOC shell to
provide command line editing and command line history recall and edit.
GNU readline (or Tecla library) must be installed on your target
system when COMMANDLINE\_LIBRARY is set to READLINE (or TECLA) for
that target. EPICS (EPICS shell) is the default specified in
CONFIG\_COMMON. A READLINE override is defined for linux-x86 in the
EPICS distribution. Comment out COMMANDLINE\_LIBRARY=READLINE in
configure/os/CONFIG\_SITE.Common.linux-x86 if readline is not
installed on linux-x86. Command-line editing and history will then be
those supplied by the os. On vxWorks the ledLib command-line input
library is used instead.

### <span id="0_0_7">Host system storage requirements</span>

The compressed tar file is approximately 1.6 MB in size. The
distribution source tree takes up approximately 12 MB. Each host
target will need around 40 MB for build files, and each cross-compiled
target around 20 MB.

### <span id="0_0_8">Documentation</span>

EPICS documentation is available through the [EPICS
website](https://epics.anl.gov/) at Argonne.

Release specific documentation can also be found in the
base/documentation directory of the distribution.

### <span id="0_0_10">Directory Structure</span>

#### Distribution directory structure:

``` 
        base                    Root directory of the base distribution
        base/configure          Operating system independent build config files
        base/configure/os       Operating system dependent build config files
        base/documentation      Distribution documentation
        base/src                Source code in various subdirectories
        base/startup            Scripts for setting up path and environment
```

#### Install directories created by the build:

``` 
        bin                     Installed scripts and executables in subdirs
        cfg                     Installed build configuration files
        db                      Installed data bases
        dbd                     Installed data base definitions
        doc                     Installed documentation files
        html                    Installed html documentation
        include                 Installed header files
        include/os              Installed os specific header files in subdirs
        include/compiler        Installed compiler-specific header files
        lib                     Installed libraries in arch subdirectories
        lib/perl                Installed perl modules
        templates               Installed templates
```

### <span id="0_0_11">Build related components</span>

#### base/documentation directory - contains setup, build, and install documents

``` 
        README.md            Instructions for setup and building epics base
        README.darwin.html   Installation notes for Mac OS X (Darwin)
        RELEASE_NOTES.html   Notes on release changes
        KnownProblems.html   List of known problems and workarounds
```

#### base/startup directory - contains scripts to set environment and path

``` 
        EpicsHostArch       Shell script to set EPICS_HOST_ARCH env variable
        unix.csh            C shell script to set path and env variables
        unix.sh             Bourne shell script to set path and env variables
        win32.bat           Bat file example to configure win32-x86 target
        windows.bat         Bat file example to configure windows-x64 target
```

#### base/configure directory - contains build definitions and rules

``` 
        CONFIG                Includes configure files and allows variable overrides
        CONFIG.CrossCommon    Cross build definitions
        CONFIG.gnuCommon      Gnu compiler build definitions for all archs
        CONFIG_ADDONS         Definitions for &lt;osclass> and DEFAULT options
        CONFIG_APP_INCLUDE    
        CONFIG_BASE           EPICS base tool and location definitions
        CONFIG_BASE_VERSION   Definitions for EPICS base version number
        CONFIG_COMMON         Definitions common to all builds
        CONFIG_ENV            Definitions of EPICS environment variables
        CONFIG_FILE_TYPE      
        CONFIG_SITE           Site specific make definitions
        CONFIG_SITE_ENV       Site defaults for EPICS environment variables
        MAKEFILE              Installs CONFIG* RULES* creates
        RELEASE               Location of external products
        RULES                 Includes appropriate rules file
        RULES.Db              Rules for database and database definition files
        RULES.ioc             Rules for application iocBoot/ioc* directory
        RULES_ARCHS           Definitions and rules for building architectures
        RULES_BUILD           Build and install rules and definitions
        RULES_DIRS            Definitions and rules for building subdirectories
        RULES_EXPAND          
        RULES_FILE_TYPE       
        RULES_TARGET          
        RULES_TOP             Rules specific to a &lt;top> dir (uninstall and tar)
        Sample.Makefile       Sample makefile with comments
```

#### base/configure/os directory - contains os-arch specific definitions

``` 
        CONFIG.&lt;host>.&lt;target>      Specific host-target build definitions
        CONFIG.Common.&lt;target>      Specific target definitions for all hosts
        CONFIG.&lt;host>.Common        Specific host definitions for all targets
        CONFIG.UnixCommon.Common    Definitions for Unix hosts and all targets
        CONFIG.Common.UnixCommon    Definitions for Unix targets and all hosts
        CONFIG.Common.vxWorksCommon Specific host definitions for all vx targets
        CONFIG_SITE.&lt;host>.&lt;target> Site specific host-target definitions
        CONFIG_SITE.Common.&lt;target> Site specific target defs for all hosts
        CONFIG_SITE.&lt;host>.Common   Site specific host defs for all targets
```

### <span id="0_0_12">Building EPICS base (Unix and Win32)</span>

#### Unpack file

Unzip and untar the distribution file. Use WinZip on Windows
systems.

#### Set environment variables

Files in the base/startup directory have been provided to help set
required path and other environment variables.

**EPICS\_HOST\_ARCH**  
Before you can build or use EPICS R3.15, the environment variable
EPICS\_HOST\_ARCH must be defined. A perl script EpicsHostArch.pl in
the base/startup directory has been provided to help set
EPICS\_HOST\_ARCH. You should have EPICS\_HOST\_ARCH set to your
host operating system followed by a dash and then your host
architecture, e.g. solaris-sparc. If you are not using the OS
vendor's c/c++ compiler for host builds, you will need another dash
followed by the alternate compiler name (e.g. "-gnu" for GNU c/c++
compilers on a solaris host or "-mingw" for MinGW c/c++ compilers on
a WIN32 host). See configure/CONFIG\_SITE for a list of supported
EPICS\_HOST\_ARCH values.

**PERLLIB**  
On WIN32, some versions of Perl require that the environment
variable PERLLIB be set to &lt;perl directory location>.

**PATH**  
As already mentioned, you must have the perl executable and you may
need C and C++ compilers in your search path. For building base you
also must have echo in your search path. For Unix host builds you
also need ln, cpp, cp, rm, mv, and mkdir in your search path and
/bin/chmod must exist. On some Unix systems you may also need ar and
ranlib in your path, and the C compiler may require as and ld in
your path. On solaris systems you need uname in your path.

**LD\_LIBRARY\_PATH**  
R3.15 shared libraries and executables normally contain the full
path to any libraries they require. However, if you move the EPICS
files or directories from their build-time location then in order
for the shared libraries to be found at runtime LD\_LIBRARY\_PATH
must include the full pathname to
$(INSTALL\_LOCATION)/lib/$(EPICS\_HOST\_ARCH) when invoking
executables, or some equivalent OS-specific mechanism (such as
/etc/ld.so.conf on Linux) must be used. Shared libraries are now
built by default on all Unix type hosts.

#### Do site-specific build configuration

**Site configuration**  
To configure EPICS, you may want to modify the default definitions
in the following files:

``` 
        configure/CONFIG_SITE      Build choices. Specify target archs.
        configure/CONFIG_SITE_ENV  Environment variable defaults
        configure/RELEASE          TORNADO2 full path location
```

**Host configuration**  
To configure each host system, you may override the default
definitions by adding a new file in the configure/os directory with
override definitions. The new file should have the same name as the
distribution file to be overridden except with CONFIG in the name
changed to CONFIG\_SITE.

``` 
        configure/os/CONFIG.&lt;host>.&lt;host>      Host build settings
        configure/os/CONFIG.&lt;host>.Common      Host common build settings
```

**Target configuration**  
To configure each target system, you may override the default
definitions by adding a new file in the configure/os directory with
override definitions. The new file should have the same name as the
distribution file to be overridden except with CONFIG in the name
replaced by CONFIG\_SITE. This step is necessary even if the host
system is the only target system.

``` 
        configure/os/CONFIG.Common.&lt;target>     Target common settings
        configure/os/CONFIG.&lt;host>.&lt;target>     Host-target settings
```

#### Build EPICS base

After configuring the build you should be able to build EPICS base
by issuing the following commands in the distribution's root
directory (base):

``` 
        gnumake clean uninstall
        gnumake
```

The command "gnumake clean uninstall" will remove all files and
directories generated by a previous build. The command "gnumake"
will build and install everything for the configured host and
targets.

It is recommended that you do a "gnumake clean uninstall" at the
root directory of an EPICS directory structure before each complete
rebuild to ensure that all components will be rebuilt.

### <span id="0_0_13">Example application and extension</span>

A perl tool, makeBaseApp.pl is included in the distribution file. This
script will create a sample application that can be built and then
executed to try out this release of base.

Instructions for building and executing the 3.15 example application
can be found in the section "Example Application" of Chapter 2,
"Getting Started", in the "IOC Application Developer's Guide" for this
release. The "Example IOC Application" section briefly explains how to
create and build an example application in a user created &lt;top>
directory. It also explains how to run the example application on a
vxWorks ioc or as a process on the host system. By running the example
application as a host-based IOC, you will be able to quickly implement
a complete EPICS system and be able to run channel access clients on
the host system.

A perl script, makeBaseExt.pl, is included in the distribution file.
This script will create a sample extension that can be built and
executed. The makeBaseApp.pl and makeBaseExt.pl scripts are installed
into the install location bin/&lt;hostarch> directory during the base
build.

### <span id="0_0_14">Multiple host platforms</span>

You can build using a single EPICS directory structure on multiple
host systems and for multiple cross target systems. The intermediate
and binary files generated by the build will be created in separate
subdirectories and installed into the appropriate separate host/target
install directories. EPICS executables and perl scripts are installed
into the `$(INSTALL_LOCATION)/bin/<arch>` directories. Libraries are
installed into $`(INSTALL_LOCATION)/lib/<arch>`. The default
definition for `$(INSTALL_LOCATION)` is `$(TOP)` which is the root
directory in the distribution directory structure, base. Created
object files are stored in O.&lt;arch> source subdirectories, This
allows objects for multiple cross target architectures to be
maintained at the same time. To build EPICS base for a specific
host/target combination you must have the proper host/target C/C++
cross compiler and target header files and the base/configure/os
directory must have the appropriate configure files.
