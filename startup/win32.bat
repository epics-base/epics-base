@ECHO OFF
set path=
set include=
set lib=
REM    --------------- WIN95 -------------------
REM set PATH=C:\WINDOWS;C:\WINDOWS\COMMAND
set PATH=C:\WINNT;C:\WINNT\system32

REM    --------------- EPICS ------------------------
set EPICS_HOST_ARCH=win32-x86
set PATH=%PATH%;G:\epics\base\bin\%EPICS_HOST_ARCH%
set PATH=%PATH%;G:\epics\extensions\bin\%EPICS_HOST_ARCH%

REM    ------------ cygwin (unix tools, make, etc.) -----
REM     need grep from here NOT from cvs directory
REM     some tools need a tmp directory
set PATH=%PATH%;.;..
REM REM  set PATH=%PATH%;c:\bin;c:\gnuwin32\b18\h-i386-cygwin32\bin
REM set PATH=%PATH%;c:\bin;c:\cygnus\B19\h-i386-cygwin32\bin
set PATH=%PATH%;c:\bin;c:\cygnus\cygwin-b20\h-i586-cygwin32\bin
set MAKEFLAGS=-w
set MAKE_MODE=unix

REM    --------------- CVS ---------------------------
set CVSROOT=/cvsroot
set LOGNAME=janet
REM         rcs needs a c:\temp directory
REM 
REM   set PATH=%PATH%;C:\cyclic\cvs-1.10\bin
set PATH=%PATH%;C:\cyclic\cvs-1.10

REM    --------------- Exceed ---------------------------
set PATH=%PATH%;C:\Exceed

REM    --------------- Epics Channel Access ----------

REM the following line works !!!!!
REM set EPICS_CA_ADDR_LIST=164.54.9.255
set EPICS_CA_ADDR_LIST=164.54.188.65 164.54.9.255

REM set EPICS_CA_AUTO_CA_ADDR_LIST=YES
REM set EPICS_CA_CONN_TMO=30.0
REM set EPICS_CA_BEACON_PERIOD=15.0
REM set EPICS_CA_REPEATER_PORT=5065
REM set EPICS_CA_SERVER_PORT=5064
REM set EPICS_TS_MIN_WEST=420

REM    --------------- perl ------------------------
REM set PERLLIB=C:\perl\lib
set PATH=%path%;C:\perl\bin

REM    --------------- vim --------------------
REM set VIM=C:\vim5.3
set PATH=%path%;C:\vim5.3

REM    --------------- JAVA -------------------
REM  set CLASSPATH=.;c:\jdk1.1.3\lib\classes.zip;
REM  set PATH=%PATH%;C:\jdk1.1.7\bin
set PATH=%PATH%;C:\jdk1.2.2\bin
set CLASSPATH=G:\epics\extensions\javalib
set CLASSPATH=%CLASSPATH%;C:\jdk1.2.2\lib\tools.jar

REM    --------------- Visual c++ ------------------

REM set PATH=%PATH%;c:\msdev\bin;
REM set PATH=%PATH%;c:\Progra~1\DevStudio\VX\bin;
REM call c:\Progra~1\DevStudio\VC\bin\vcvars32.bat
REM call c:\msdev\bin\vcvars32.bat m68k

REM ----- Visual c++ 6.0 ---------
call "D:\Program files\Microsoft Visual Studio\Vc98\bin\vcvars32.bat"
REM ----- Visual c++ 5.0 ---------
REM call c:\epics\vcvars32.bat m68k
REM ----- Visual c++ 4.2 ---------
REM call c:\msdev\bin\vcvars32.bat m68k

REM    -----------------------------------------------

