@ECHO OFF
REM ---------------- cygwin32 ------------------
REM         A \tmp directory is needed
REM         \bin needed in path before windows dirs
REM         . needed in path by bash

set PATH=c:\bin;c:\usr\local\h-i386-cygwin32\bin;.

REM -------------- windows ------------------

set PATH=%PATH%;C:\WINDOWS;C:\WINDOWS\COMMAND

REM ---------------CVS-------------------
REM  Cvs settings (not needed for base build)
REM         rcs needs a c:\temp directory
REM set CVSROOT=/cvsroot
REM set LOGNAME=janet
REM set PATH=%PATH%;C:\cyclic\cvs-1.9\bin;


REM -----------------extra path dirs -----------------
REM  Extra path settings (not needed for base build)
REM
REM   For 'which' and other unix tools not included in cygwin32
REM         set PATH=%PATH%;C:\unix\VIRTUNIX\32-BIT;C:\VIRTUNIX\16-BIT
REM         set PATH=%PATH%;C:\unix\UTEXAS\BIN
REM         set PATH=%PATH%;c:\util

REM ----------------- epics -----------------

set HOST_ARCH=cygwin32

REM ----------------------------------
REM Following 3 defs may be needed if cygwin32  not in \usr\local
REM 
REM  set C_INCLUDE_PATH=
REM  set CPLUS_INCLUDE_PATH=
REM  set LIBRARY_PATH=
REM ----------------------------------


REM ----------------- cygwin32: gcc -----------------
REM  Trailing slash is important

set GCC_EXEC_PREFIX=C:\usr\local\H-i386-cygwin32\lib\gcc-lib\

REM ----------------- cygwin32: bash  -----------------
REM  bash settings (not needed for base build)
REM
REM        TERMCAP, HOME, and TERM needed for bash (.bashrc)
REM        set TERMCAP=/usr/local/h-i386-cygwin32/etc/termcap
REM        set TERM=ansi
REM        set HOME=/

REM ------------ Channel Access ---------------
REM  Channel access settings (not needed for base build)
REM
REM set EPICS_CA_ADDR_LIST=""
REM set EPICS_CA_AUTO_CA_ADDR_LIST=YES
REM set EPICS_CA_CONN_TMO=30.0
REM set EPICS_CA_BEACON_PERIOD=15.0
REM set EPICS_CA_REPEATER_PORT=5065
REM set EPICS_CA_SERVER_PORT=5064
REM set EPICS_TS_MIN_WEST=420

REM    --------------- perl ------------------------

set PATH=%path%;C:\perl\pw32i304\bin
set PERLLIB=C:\perl\pw32i304\lib

REM    --------------- tcl ------------------------
REM  tcl/tk settings ( not needed for base build)
REM
REM set PATH=%PATH%;C:\usr\local\tcl\bin
REM  Forward slashes in following 2 lines are important
REM set TCL_LIBRARY=C:/usr/local/tcl/lib/tcl7.6
REM set GDBTK_LIBRARY=C:/usr/local/share/gdbtcl


