@ECHO OFF

REM ---------------- cygnus B19 -----------------
REM         A \tmp directory is needed
REM         \bin with sh.exe is needed by some tools
REM         . needed in path by sh

SET MAKE_MODE=unix
SET CYGFS=C:/Cygnus/B19
SET GCC_EXEC_PREFIX=C:\CYGNUS\B19\H-I386~1\lib\gcc-lib\
SET PATH=C:\CYGNUS\B19\H-I386~1\bin;.;c:\bin

REM ---------------- windows --------------------
set PATH=%PATH%;C:\WINDOWS;C:\WINDOWS\COMMAND

REM ---------------- CVS-------------------------
REM         rcs needs a c:\temp directory
set CVSROOT=/cvsroot
set LOGNAME=janet
set PATH=%PATH%;C:\cyclic\cvs-1.9\bin

REM  --------------- perl -----------------------
REM set PERLLIB=C:\perl\pw32i310\lib
set PATH=%path%;C:\perl\pw32i310\bin

REM ---------------- epics ----------------------
set HOST_ARCH=cygwin32
set PATH=%PATH%;F:\epics\base\bin\cygwin32

REM ---------------- Channel Access -------------
REM set EPICS_CA_ADDR_LIST=164.54.9.255

REM   set EPICS_CA_AUTO_CA_ADDR_LIST=YES
REM   set EPICS_CA_CONN_TMO=30.0
REM   set EPICS_CA_BEACON_PERIOD=15.0
REM   set EPICS_CA_REPEATER_PORT=5065
REM   set EPICS_CA_SERVER_PORT=5064
REM   set EPICS_TS_MIN_WEST=420

REM ---------------- tk/tcl ---------------------
SET TCL_LIBRARY=%CYGFS%\share\tcl8.0\
SET GDBTK_LIBRARY=%CYGFS%/share/gdbtcl

REM ---------------- JAVA -----------------------
REM   set CLASSPATH=.;c:\jdk1.1.6\lib\classes.zip;
set PATH=%PATH%;C:\jdk1.1.6\bin

REM ---------------- extra optional dirs --------
REM   set PATH=%PATH%;C:\unix\VIRTUNIX\32-BIT;C:\VIRTUNIX\16-BIT
REM   set PATH=%PATH%;C:\unix\UTEXAS\BIN
REM   set PATH=%PATH%;c:\util

