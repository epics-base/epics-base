@ECHO OFF
REM    ===================================================
REM    ====== REQUIRED ENVIRONMENT VARIABLES FOLLOW ======
REM
REM    --------------- WINDOWS ---------------------------
REM    ----- WIN95 -----
REM set PATH=C:\WINDOWS;C:\WINDOWS\COMMAND
REM    ----- WINNT -----
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

REM     ------------ Borland c++ compiler -----------------
REM     cygwin tools first so borland make not used
set PATH=%PATH%;c:\Borland\Bcc55\bin

REM    --------------- EPICS -----------------------------
REM    R3.14 requirements
set EPICS_HOST_ARCH=win32-x86-borland
set PATH=%PATH%;G:\epics\base\bin\%EPICS_HOST_ARCH%
set PATH=%PATH%;G:\epics\extensions\bin\%EPICS_HOST_ARCH%

REM    ===================================================
REM    ====== OPTIONAL ENVIRONMENT VARIABLES FOLLOW ======

REM    --------------- GNU make flags --------------------
REM set MAKEFLAGS=-w

REM    --------------- EPICS Channel Access --------------
REM    Uncomment and modify the following lines
REM    to override the base/configure/CONFIG_ENV defaults
REM set EPICS_CA_ADDR_LIST=164.54.188.65 164.54.5.255
REM set EPICS_CA_AUTO_ADDR_LIST=YES

REM    --------------- vim (use cygwin vim ) ----------------
REM HOME needed by vim to write .viminfo file.
set HOME=c:/users/%USERNAME%
REM VIM needed by vim to find _vimrc file.
set VIM=c:\cygwin

REM    --------------- JAVA ------------------------------
REM    Needed for java extensions
set PATH=%PATH%;C:\j2sdk1.4.0\bin
set CLASSPATH=G:\epics\extensions\javalib

REM    --------------- Exceed ----------------------------
REM    Needed for XWindow extensions
REM    Cygwin should preceed Exceed in path
REM set PATH=%PATH%;C:\Exceed

REM    ===================================================
