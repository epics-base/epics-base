@ECHO OFF
REM *************************************************************************
REM  Copyright (c) 2002 The University of Chicago, as Operator of Argonne
REM      National Laboratory.
REM  Copyright (c) 2002 The Regents of the University of California, as
REM      Operator of Los Alamos National Laboratory.
REM  EPICS BASE Versions 3.13.7
REM  and higher are distributed subject to a Software License Agreement found
REM  in file LICENSE that is included with this distribution.
REM *************************************************************************

REM    ===================================================
REM    ====== REQUIRED ENVIRONMENT VARIABLES FOLLOW ======
REM
REM    --------------- WINDOWS ---------------------------
REM    ----- WIN95 -----
REM set PATH=C:\WINDOWS;C:\WINDOWS\COMMAND
REM    ----- WINNT -----
set PATH=C:\WINNT;C:\WINNT\SYSTEM32

REM    --------------- GNU tools -------------------------
REM    cygwin contains tk/tcl, vim, perl, and many unix tools
set PATH=%PATH%;c:\cygwin\bin

REM    --------------- EPICS -----------------------------
REM    R3.14 requirements
set EPICS_HOST_ARCH=cygwin-x86
set PATH=%PATH%;G:\epics\base_R3_14_2_branch\bin\%EPICS_HOST_ARCH%
set PATH=%PATH%;G:\epics\extensions\bin\%EPICS_HOST_ARCH%

REM    ===================================================
REM    ====== OPTIONAL ENVIRONMENT VARIABLES FOLLOW ======

REM    ---------------- EPICS tools ----------------------
REM HOST_ARCH for Makefile.Host builds
set HOST_ARCH=cygwin32

REM    --------------- GNU make flags --------------------
REM set MAKEFLAGS=-w

REM    --------------- EPICS Channel Access --------------
REM    Uncomment and modify the following lines
REM    to override the base/configure/CONFIG_ENV defaults
REM set EPICS_CA_ADDR_LIST= n.n.n.n n.n.n.n
REM set EPICS_CA_AUTO_ADDR_LIST=YES

REM    --------------- cygwin vim ------------------------
REM HOME needed by vim to find _vimrc file.
set HOME=/home/%USERNAME%
REM VIM needed by vim to find help files.
set VIM=/usr/share/vim/vim61

REM    --------------- remote cvs (use cygwin cvs) -------
REM HOME needed by cvs for .cvsrc file (set in vim above) 
set CVSROOT=:ext:%USERNAME%@venus.aps.anl.gov:/usr/local/epicsmgr/cvsroot
set CVS_RSH=/bin/ssh.exe

REM    --------------- JAVA ------------------------------
REM    Needed for java extensions
set PATH=%PATH%;C:\j2sdk1.4.1_01\bin
set CLASSPATH=G:\epics\extensions\javalib

REM    --------------- X11+Motif--------------------------
REM    Exceed or cygwin Xfree86 needed for Xwindows extensions
REM
REM    Exceed ( Cygwin should preceed Exceed in path)
REM set PATH=%PATH%;C:\Exceed
REM    or
REM    cygwin Xfree86
set PATH=%PATH%;c:\cygwin\usr\X11R6\bin
set DISPLAY=localhost:0

REM    ===================================================
