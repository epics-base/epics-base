@ECHO OFF
set path=
set include=
set lib=
REM    ===================================================
REM    ====== REQUIRED ENVIRONMENT VARIABLES FOLLOW ======
REM
REM    --------------- WINDOWS ---------------------------
REM    ----- WIN95 -----
REM set PATH=C:\WINDOWS;C:\WINDOWS\COMMAND
REM    ----- WINNT, WIN2000 -----
set PATH=C:\WINNT;C:\WINNT\SYSTEM32

REM    --------------- GNU make (use cygwin make ) ----------------
REM    Needed only if using GNU make from cygwin
REM    Can be replaced with path to a GNU make
REM
REM    cygwin contains tk/tcl, vim, perl, and many unix tools
REM    need grep from here NOT from cvs directory
REM    some tools may need a tmp directory
set PATH=%PATH%;c:\cygwin\bin

REM    --------------- perl (use cygwin perl) ------------
set PERLLIB=/C//cygwin/lib/perl5/5.6.1

REM    --------------- Visual c++ ------------------------
REM    ----- Visual c++ 6.0 -----
call "C:\Program files\Microsoft Visual Studio\Vc98\bin\vcvars32.bat"

REM    --------------- EPICS -----------------------------
REM ----- R3.13 EPICS -----
set HOST_ARCH=WIN32
set PATH=%PATH%;G:\epics\base_R3_13_1_branch\bin\%HOST_ARCH%
set PATH=%PATH%;G:\epics\extensions\bin\%HOST_ARCH%

REM    ===================================================
REM    ====== OPTIONAL ENVIRONMENT VARIABLES FOLLOW ======

REM    --------------- GNU make flags --------------------
REM set MAKEFLAGS=-w

REM    --------------- cygwin vim ------------------------
REM HOME needed by vim to find _vimrc file.
set HOME=/home/%USERNAME%
REM VIM needed by vim to find help files.
set VIM=/usr/share/vim/vim61

REM    --------------- cygwin cvs  ------------------------
REM HOME needed by cvs for .cvsrc file (set in vim above) 
set CVSROOT=:ext:jba@venus.aps.anl.gov:/usr/local/epicsmgr/cvsroot
set CVS_RSH=/bin/ssh.exe

REM    --------------- EPICS Channel Access --------------
REM    Uncomment and modify the following lines
REM    to override the base/configure/CONFIG_ENV defaults
REM set EPICS_CA_ADDR_LIST=
REM set EPICS_CA_AUTO_CA_ADDR_LIST=YES

REM    --------------- JAVA ------------------------------
REM    Needed for java extensions
set PATH=C:\j2sdk1.4.1_01\bin;%PATH%
set CLASSPATH=G:\epics\extensions\javalib

REM    --------------- Xfree86 + Lesstif ----------------------
REM Need X11R6\bin in path to get dlls
set PATH=%PATH%;c:\cygwin\usr\X11R6\bin

REM NOTE: Need to start X server by hand before executing extensions
REM Command:   startxwin
REM or
REM NOTE: Use /B option on NT and WIN2000 only
REM start /B XWin -screen 0 1024 768
REM start /B twm

REM    --------------- Exceed 7.00 ----------------------
REM -- Exceed xdk 7.00 ----
REM set PATH=%PATH%;C:\Progra~1\Hummingbird\Connectivity\7.00\Exceed
REM -- Exceed xdk 6.01 ----
REM NOTE: Need to start Exceed by hand before executing extensions
REM set DISPLAY=localhost:0
REM set PATH=%PATH%;C:\Exceed\dlls

REM    --------------- CVS ---------------------------
REM set CVSROOT=/cvsroot
REM set LOGNAME=janet
REM         rcs needs a c:\temp directory
REM
REM set PATH=%PATH%;C:\cyclic\cvs-1.10

REM ---------------- tk/tcl ---------------------
REM SET CYGFS=C:/cygwin
REM SET TCL_LIBRARY=%CYGFS%\share\tcl8.0\
REM SET GDBTK_LIBRARY=%CYGFS%/share/gdbtcl

