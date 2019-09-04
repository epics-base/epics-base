<!DOCTYPE HTML>
<!-- Generate the README.1st file from this file using:
    elinks -dump -no-numbering -no-references -dump-width 80 README.html
  -->
<HTML>
<HEAD>
<TITLE>README - EPICS Base Installation Instructions</TITLE>
</HEAD>
<BODY>
<CENTER>
<H1>Installation Instructions</H1>
<H2>EPICS Base Release 3.15.6</H2><BR>
</CENTER>
<HR>
<H3> Table of Contents</H3>
<UL>
<LI><A HREF="#0_0_1"> What is EPICS base?</A></LI>
<LI><A HREF="#0_0_2"> What is new in this release?</A></LI>
<LI><A HREF="#0_0_3"> Copyright</A></LI>
<LI><A HREF="#0_0_4"> Supported platforms</A></LI>
<LI><A HREF="#0_0_5"> Supported compilers</A></LI>
<LI><A HREF="#0_0_6"> Software requirements</A></LI>
<LI><A HREF="#0_0_7"> Host system storage requirements</A></LI>
<LI><A HREF="#0_0_8"> Documentation</A></LI>
<LI><A HREF="#0_0_10"> Directory Structure</A></LI>
<LI><A HREF="#0_0_11"> Build related components</A></LI>
<LI><A HREF="#0_0_12"> Building EPICS base (Unix and Win32)</A></LI>
<LI><A HREF="#0_0_13"> Example application and extension</A></LI>
<LI><A HREF="#0_0_14"> Multiple host platforms</A></LI>
</UL>
<HR>

<H3><A NAME="0_0_1"> What is EPICS base?</A></H3>
<BLOCKQUOTE>The Experimental Physics and Industrial Control Systems
 (EPICS) is an extensible set of software components and tools with
 which application developers can create a control system. This control
 system can be used to control accelerators, detectors, telescopes, or
 other scientific experimental equipment. EPICS base is the set of core
 software, i.e. the components of EPICS without which EPICS would not
 function. EPICS base allows an arbitrary number of target systems, IOCs
 (input/output controllers), and host systems, OPIs (operator
 interfaces) of various types.</BLOCKQUOTE>

<H3><A NAME="0_0_2"> What is new in this release?</A></H3>
<BLOCKQUOTE> Please check the RELEASE_NOTES file in the distribution for
 description of changes and release migration details.</BLOCKQUOTE>

<H3><A NAME="0_0_3"> Copyright</A></H3>
<BLOCKQUOTE>Please review the LICENSE file included in the
 distribution for legal terms of usage.</BLOCKQUOTE>

<H3><A NAME="0_0_4"> Supported platforms</A></H3>

<BLOCKQUOTE>The list of platforms supported by this version of EPICS base
 is given in the configure/CONFIG_SITE file. If you are trying to build
 EPICS Base on an unlisted host or for a different target machine you
 must have the proper host/target cross compiler and header files, and
 you will have to create and add the appropriate new configure files to
 the base/configure/os/directory. You can start by copying existing
 configuration files in the configure/os directory and then make changes
 for your new platforms.</BLOCKQUOTE>

<H3><A NAME="0_0_5"> Supported compilers</A></H3>

<BLOCKQUOTE>This version of EPICS base has been built and tested using the host
 vendor's C and C++ compilers, as well as the GNU gcc and g++ compilers. The GNU
 cross-compilers work for all cross-compiled targets. You may need the C and C++
 compilers to be in your search path to do EPICS builds; check the definitions
 of CC and CCC in base/configure/os/CONFIG.&lt;host&gt;.&lt;host&gt; if you have
 problems.</BLOCKQUOTE>

<H3><A NAME="0_0_6"> Software requirements</A></H3>

<BLOCKQUOTE><B>GNU make</B><BR>
 You must use GNU make, gnumake, for any EPICS builds. Set your path
 so that a gnumake version 3.81 or later is available.

<P><B>Perl</B><BR>
 You must have Perl version 5.8.1 or later installed. The EPICS configuration
 files do not specify the perl full pathname, so the perl executable must
 be found through your normal search path.</P>

<P><B>Unzip and tar (Winzip on WIN32 systems)</B><BR>
 You must have tools available to unzip and untar the EPICS base
 distribution file.</P>

<P><B>Target systems</B><BR>
 EPICS supports IOCs running on embedded platforms such as VxWorks
 and RTEMS built using a cross-compiler, and also supports soft IOCs running
 as processes on the host platform.</P>

<P><B>vxWorks</B><BR>
 You must have vxWorks 5.5.x or 6.x installed if any of your target systems are
 vxWorks systems; the C++ compiler for vxWorks 5.4 is now too old to support.
 The vxWorks installation provides the cross-compiler and header files needed to
 build for these targets. The absolute path to and the version number of the
 vxWorks installation must be set in the
 base/configure/os/CONFIG_SITE.Common.vxWorksCommon file or in one of its
 target-specific overrides.</P>

<P>Consult the <a href="https://epics.anl.gov/base/tornado.php">vxWorks
 5.x</a> or <a href="https://epics.anl.gov/base/vxWorks6.php">vxWorks
 6.x</a> EPICS web pages about and the vxWorks documentation for information
 about configuring your vxWorks operating system for use with EPICS.</P>

<P><B>RTEMS</B><BR>
 For RTEMS targets, you need RTEMS core and toolset version 4.9.2 or later.</P>

<P><B>GNU readline or Tecla library</B><BR>
 GNU readline and Tecla libraries can be used by the IOC shell to
 provide command line editing and command line history recall and edit.
 GNU readline (or Tecla library) must be installed on your target system
 when COMMANDLINE_LIBRARY is set to READLINE (or TECLA) for that target.
 EPICS (EPICS shell) is the default specified in CONFIG_COMMON. A
 READLINE override is defined for linux-x86 in the EPICS distribution.
 Comment out COMMANDLINE_LIBRARY=READLINE in
 configure/os/CONFIG_SITE.Common.linux-x86 if readline is not installed
 on linux-x86. Command-line editing and history will then be those
 supplied by the os. On vxWorks the ledLib command-line input library is
 used instead.</P>
</BLOCKQUOTE>

<H3><A NAME="0_0_7"> Host system storage requirements</A></H3>

<BLOCKQUOTE>The compressed tar file is approximately 1.6 MB in size. The
 distribution source tree takes up approximately 12 MB. Each host target will
 need around 40 MB for build files, and each cross-compiled target around 20
 MB.</BLOCKQUOTE>

<H3><A NAME="0_0_8"> Documentation</A></H3>
<BLOCKQUOTE>EPICS documentation is available through the
 <a href="https://epics.anl.gov/">EPICS website</a> at Argonne.
 <P>Release specific documentation can also be found in the base/documentation
 directory of the distribution.</BLOCKQUOTE>

<H3><A NAME="0_0_10"> Directory Structure</A></H3>
<BLOCKQUOTE><H4>Distribution directory structure:</H4>

<PRE>
        base                    Root directory of the base distribution
        base/configure          Operating system independent build config files
        base/configure/os       Operating system dependent build config files
        base/documentation      Distribution documentation
        base/src                Source code in various subdirectories
        base/startup            Scripts for setting up path and environment
</PRE>

<H4>Install directories created by the build:</H4>
<PRE>
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
</PRE>
</BLOCKQUOTE>

<H3><A NAME="0_0_11"> Build related components</A></H3>
<BLOCKQUOTE>

<H4>base/documentation directory - contains setup, build, and install
 documents</H4>
 <PRE>
        README.1st           Instructions for setup and building epics base
        README.html          html version of README.1st
        README.darwin.html   Installation notes for Mac OS X (Darwin)
        RELEASE_NOTES.html   Notes on release changes
        KnownProblems.html   List of known problems and workarounds
</PRE>

<H4>base/startup directory - contains scripts to set environment and path</H4>
<PRE>
        EpicsHostArch       Shell script to set EPICS_HOST_ARCH env variable
        unix.csh            C shell script to set path and env variables
        unix.sh             Bourne shell script to set path and env variables
        win32.bat           Bat file example to configure win32-x86 target
        windows.bat         Bat file example to configure windows-x64 target
</PRE>

<H4>base/configure directory - contains build definitions and rules</H4>
<PRE>
        CONFIG                Includes configure files and allows variable overrides
        CONFIG.CrossCommon    Cross build definitions
        CONFIG.gnuCommon      Gnu compiler build definitions for all archs
        CONFIG_ADDONS         Definitions for &lt;osclass&gt; and DEFAULT options
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
        RULES_TOP             Rules specific to a &lt;top&gt; dir (uninstall and tar)
        Sample.Makefile       Sample makefile with comments
</PRE>

<H4>base/configure/os directory - contains os-arch specific definitions</H4>
<PRE>
        CONFIG.&lt;host&gt;.&lt;target&gt;      Specific host-target build definitions
        CONFIG.Common.&lt;target&gt;      Specific target definitions for all hosts
        CONFIG.&lt;host&gt;.Common        Specific host definitions for all targets
        CONFIG.UnixCommon.Common    Definitions for Unix hosts and all targets
        CONFIG.Common.UnixCommon    Definitions for Unix targets and all hosts
        CONFIG.Common.vxWorksCommon Specific host definitions for all vx targets
        CONFIG_SITE.&lt;host&gt;.&lt;target&gt; Site specific host-target definitions
        CONFIG_SITE.Common.&lt;target&gt; Site specific target defs for all hosts
        CONFIG_SITE.&lt;host&gt;.Common   Site specific host defs for all targets
</PRE>

</BLOCKQUOTE>

<H3><A NAME="0_0_12"> Building EPICS base (Unix and Win32)</A></H3>
<BLOCKQUOTE>

<H4> Unpack file</H4>
<BLOCKQUOTE>
Unzip and untar the distribution file. Use WinZip on Windows
 systems.
</BLOCKQUOTE>

<H4>Set environment variables</H4>
<BLOCKQUOTE>
Files in the base/startup directory have been provided to
 help set required path and other environment variables.

<P><B>EPICS_HOST_ARCH</B><BR>
 Before you can build or use EPICS R3.15, the environment variable
 EPICS_HOST_ARCH must be defined. A perl script EpicsHostArch.pl in the
 base/startup directory has been provided to help set EPICS_HOST_ARCH.
 You should have EPICS_HOST_ARCH set to your host operating system
 followed by a dash and then your host architecture, e.g. solaris-sparc.
 If you are not using the OS vendor's c/c++ compiler for host builds,
 you will need another dash followed by the alternate compiler name
 (e.g. &quot;-gnu&quot; for GNU c/c++ compilers on a solaris host or &quot;-mingw&quot;
 for MinGW c/c++ compilers on a WIN32 host). See configure/CONFIG_SITE
 for a list of supported EPICS_HOST_ARCH values.</P>

<P><B>PERLLIB</B><BR>
 On WIN32, some versions of Perl require that the environment
 variable PERLLIB be set to &lt;perl directory location&gt;.</P>

<P><B>PATH</B><BR>
 As already mentioned, you must have the perl executable and you may
 need C and C++ compilers in your search path. For building base you
 also must have echo in your search path. For Unix host builds you also
 need ln, cpp, cp, rm, mv, and mkdir in your search path and /bin/chmod
 must exist. On some Unix systems you may also need ar and ranlib in
 your path, and the C compiler may require as and ld in your path. On
 solaris systems you need uname in your path.</P>

<P><B>LD_LIBRARY_PATH</B><BR>

 R3.15 shared libraries and executables normally contain the full path
 to any libraries they require.
 However, if you move the EPICS files or directories from their build-time
 location then in order for the shared libraries to be found at runtime
 LD_LIBRARY_PATH must include the full pathname to
 $(INSTALL_LOCATION)/lib/$(EPICS_HOST_ARCH) when invoking executables, or
 some equivalent OS-specific mechanism (such as /etc/ld.so.conf on Linux)
 must be used.
 Shared libraries are now built by default on all Unix type hosts.</P>
</BLOCKQUOTE>

<H4>Do site-specific build configuration</H4>
<BLOCKQUOTE>

<B>Site configuration</B><BR>
 To configure EPICS, you may want to modify the default definitions
 in the following files:
<PRE>
        configure/CONFIG_SITE      Build choices. Specify target archs.
        configure/CONFIG_SITE_ENV  Environment variable defaults
        configure/RELEASE          TORNADO2 full path location
</PRE>

<B> Host configuration</B><BR>
 To configure each host system, you may override the default
 definitions by adding a new file in the configure/os directory with
 override definitions. The new file should have the same name as the
 distribution file to be overridden except with CONFIG in the name
 changed to CONFIG_SITE.

<PRE>
        configure/os/CONFIG.&lt;host&gt;.&lt;host&gt;      Host build settings
        configure/os/CONFIG.&lt;host&gt;.Common      Host common build settings
</PRE>

<B>Target configuration</B><BR>
 To configure each target system, you may override the default
 definitions by adding a new file in the configure/os directory with
 override definitions. The new file should have the same name as the
 distribution file to be overridden except with CONFIG in the name
 replaced by CONFIG_SITE. This step is necessary even if the host system
 is the only target system.
 <PRE>
        configure/os/CONFIG.Common.&lt;target&gt;     Target common settings
        configure/os/CONFIG.&lt;host&gt;.&lt;target&gt;     Host-target settings
</PRE>
</BLOCKQUOTE>

<H4>Build EPICS base</H4>
<BLOCKQUOTE>After configuring the build you should be able to build
 EPICS base by issuing the following commands in the distribution's root
 directory (base):
 <PRE>
        gnumake clean uninstall
        gnumake
</PRE>

 The command &quot;gnumake clean uninstall&quot;
 will remove all files and directories generated by a previous build.
 The command &quot;gnumake&quot; will build and install everything for the
 configured host and targets.

<P> It is recommended that you do a &quot;gnumake clean uninstall&quot; at the
 root directory of an EPICS directory structure before each complete
 rebuild to ensure that all components will be rebuilt.
</BLOCKQUOTE>
</BLOCKQUOTE>

<H3><A NAME="0_0_13"> Example application and extension</A></H3>
<BLOCKQUOTE>A perl tool, makeBaseApp.pl is included in the distribution
 file. This script will create a sample application that can be built
 and then executed to try out this release of base. 

<P>
 Instructions for building and executing the 3.15 example application
 can be found in the section &quot;Example Application&quot; of Chapter 2,
 &quot;Getting Started&quot;, in the &quot;IOC Application Developer's Guide&quot; for this
 release. The &quot;Example IOC Application&quot; section briefly explains how to
 create and build an example application in a user created &lt;top&gt;
 directory. It also explains how to run the example application on a
 vxWorks ioc or as a process on the host system.
 By running the example application as a host-based IOC, you will be 
 able to quickly implement a complete EPICS system and be able to run channel 
 access clients on the host system.

<P>
 A perl script,
 makeBaseExt.pl, is included in the distribution file. This script will
 create a sample extension that can be built and executed. The
 makeBaseApp.pl and makeBaseExt.pl scripts are installed into the
 install location bin/&lt;hostarch&gt; directory during the base build.
</BLOCKQUOTE>

<H3><A NAME="0_0_14"> Multiple host platforms</A></H3>
<BLOCKQUOTE>You can build using a single EPICS directory structure on
 multiple host systems and for multiple cross target systems. The
 intermediate and binary files generated by the build will be created in
 separate subdirectories and installed into the appropriate separate
 host/target install directories. EPICS executables and perl scripts are
 installed into the <TT>$(INSTALL_LOCATION)/bin/&lt;arch&gt;</TT> directories.
 Libraries are installed into $<TT>(INSTALL_LOCATION)/lib/&lt;arch&gt;</TT>.
 The default definition for <TT>$(INSTALL_LOCATION)</TT> is <TT>$(TOP)</TT>
 which is the root directory in the distribution directory structure,
 base. Created object files are stored in O.&lt;arch&gt; source
 subdirectories, This allows objects for multiple cross target
 architectures to be maintained at the same time. To build EPICS base
 for a specific host/target combination you must have the proper
 host/target C/C++ cross compiler and target header files and the
 base/configure/os directory must have the appropriate configure files.
</BLOCKQUOTE>
</BODY>
</HTML>
