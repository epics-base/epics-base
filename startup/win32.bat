@ECHO OFF
REM    ===================================================
REM    ====== REQUIRED ENVIRONMENT VARIABLES FOLLOW ======
REM
REM    --------------- WINDOWS ---------------------------
REM    ----- WIN95 -----
REM set PATH=C:\WINDOWS;C:\WINDOWS\COMMAND
REM    ----- WINNT -----
set PAth=c:\wiNNT;C:\WINNT\system32

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
call "D:\Program files\Microsoft Visual Studio\Vc98\bin\vcvars32.bat"

REM    --------------- EPICS -----------------------------
set EPICS_HOST_ARCH=win32-x86
set PATH=%PATH%;G:\epics\base\bin\%EPICS_HOST_ARCH%
set PATH=%PATH%;G:\epics\extensions\bin\%EPICS_HOST_ARCH%

REM    ===================================================
REM    ====== OPTIONAL ENVIRONMENT VARIABLES FOLLOW ======

REM    --------------- GNU make flags --------------------
REM set MAKEFLAGS=-w

REM    --------------- EPICS Channel Access --------------
REM    Uncomment and modify the following lines
REM    to override the base/configure/CONFIG_ENV defaults
REM set EPICS_CA_ADDR_LIST=164.54.5.255
REM set EPICS_CA_ADDR_LIST=164.54.188.65 164.54.5.255
REM set EPICS_CA_AUTO_CA_ADDR_LIST=YES

REM    --------------- JAVA ------------------------------
REM    Needed for java extensions
REM set PATH=%PATH%;C:\jdk1.2.2\bin
REM set CLASSPATH=G:\epics\extensions\javalib
REM set CLASSPATH=%CLASSPATH%;C:\jdk1.2.2\lib\tools.jar

REM    --------------- Exceed ----------------------------
REM    Needed for XWindow extensions
REM set PATH=%PATH%;C:\Exceed

REM    ===================================================
