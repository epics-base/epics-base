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
REM  $Revision-Id$
REM
REM  Site-specific EPICS environment settings
REM 
REM  sites should modify these definitions


REM    ===================================================
REM    ====== REQUIRED ENVIRONMENT VARIABLES FOLLOW ======
REM
REM    --------------- WINDOWS ---------------------------
REM    ----- WIN95 -----
REM set PATH=C:\WINDOWS;C:\WINDOWS\COMMAND
REM    ----- WINNT -----
REM set PATH=C:\WINNT;C:\WINNT\SYSTEM32
REM    ----- WINXP -----
set PATH=C:\WINDOWS\system32;C:\WINDOWS;C:\WINDOWS\system32\Wbem

REM    --------------- GNU tools -------------------------
REM    -- cygwin contains GNU make, perl, tk/tcl, vim, ...
REM    -- Can be preceeded or replaced with paths to GNU make and perl
REM    -- need grep from here NOT from cvs directory
REM    -- some tools may need a tmp directory
set PATH=%PATH%;c:\cygwin\bin

REM    --------------- Visual c++ ------------------------
REM    ---- Visual c++ 6.0 ------
REM call "C:\Program files\Microsoft Visual Studio\Vc98\bin\vcvars32.bat"
REM    ---- Visual Studio .NET 2003 ------
REM call "C:\Program files\Microsoft Visual Studio .NET 2003\Vc7\bin\vcvars32.bat"
REM    ---- Visual Studio 2005 -----
REM call "C:\Program files\Microsoft Visual Studio 8\VC\bin\vcvars32.bat"
REM    ---- Visual Studio 2008 -----
call "C:\Program files\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat"


REM    --------------- EPICS -----------------------------
REM    -- R3.14 requirements
set EPICS_HOST_ARCH=win32-x86
set PATH=%PATH%;G:\epics\base\bin\%EPICS_HOST_ARCH%
set PATH=%PATH%;G:\epics\extensions\bin\%EPICS_HOST_ARCH%

REM    ===================================================
REM    ====== OPTIONAL ENVIRONMENT VARIABLES FOLLOW ======

REM    ---------------- EPICS tools ----------------------
REM    -- HOST_ARCH needed for Makefile.Host builds --
set HOST_ARCH=WIN32

REM    --------------- GNU make flags --------------------
REM set MAKEFLAGS=-w

REM    --------------- EPICS Channel Access --------------
REM    -- Uncomment and modify the following lines
REM    -- to override the base/configure/CONFIG_ENV defaults
REM set EPICS_CA_ADDR_LIST=n.n.n.n n.n.n.n
REM set EPICS_CA_AUTO_ADDR_LIST=YES

REM    --------------- cygwin vim ------------------------
REM    -- HOME needed by vim to find _vimrc file.
REM set HOME=/home/%USERNAME%
REM    -- VIM needed by vim to find help files.
REM set VIM=/usr/share/vim/vim61

REM    --------------- remote cvs (use cygwin cvs) -------
REM    -- HOME needed by cvs for .cvsrc file (set in vim above) 
REM set CVSROOT=:ext:%USERNAME%@venus.aps.anl.gov:/usr/local/epicsmgr/cvsroot
REM set CVS_RSH=/bin/ssh.exe

REM    --------------- JAVA ------------------------------
REM    -- Needed for java extensions
REM set PATH=%PATH%;C:\j2sdk1.4.1_01\bin
REM set CLASSPATH=G:\epics\extensions\javalib

REM    --------------- X11+Motif--------------------------
REM    -- Exceed or cygwin Xfree86 needed for Xwindows extensions
REM
REM    -- Exceed ( Cygwin should preceed Exceed in path)
REM set PATH=%PATH%;C:\Exceed
REM    ------ Exceed 2007 ------
REM set PATH=%PATH%;C:\Program Files\Hummingbird\Connectivity\12.00\Exceed\
REM    ------ Exceed 2008 ------
REM set PATH=%PATH%;C:\Program Files\Hummingbird\Connectivity\13.00\Exceed\
REM    --
REM    -- or
REM    ----- cygwin Xfree86 -----
REM set PATH=%PATH%;c:\cygwin\usr\X11R6\bin
REM set DISPLAY=localhost:0

REM    ===================================================
