@ECHO OFF
REM *************************************************************************
REM  Copyright (c) 2017 UChicago Argonne LLC, as Operator of Argonne
REM      National Laboratory.
REM  Copyright (c) 2002 The Regents of the University of California, as
REM      Operator of Los Alamos National Laboratory.
REM  EPICS BASE is distributed subject to a Software License Agreement found
REM  in file LICENSE that is included with this distribution.
REM *************************************************************************
REM
REM  EPICS build configuration environment settings
REM
REM  Installers should modify these definitions as appropriate.
REM  This file configures the PATH variable from scratch.

REM ======================================================
REM   ====== REQUIRED ENVIRONMENT VARIABLES FOLLOW =====
REM ======================================================

REM ======================================================
REM   ---------------- WINDOWS -------------------------
REM ======================================================
REM ----- WINXP, Vista, Windows 7 -----
set PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem

REM ======================================================
REM   --------------- Strawberry Perl ------------------
REM ======================================================

set PATH=C:\Strawberry\perl\bin;%PATH%
set PATH=C:\Strawberry\perl\site\bin;%PATH%
set PATH=C:\Strawberry\c\bin;%PATH%

REM ======================================================
REM   --------------- Visual C++ -----------------------
REM ======================================================
REM --  windows-x64 ---

REM   ----- Visual Studio 2010 -----
REM call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" x64

REM   ----- Visual Studio 2015 -----
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64

REM ======================================================
REM   --------------- EPICS Base -----------------------
REM ======================================================
set EPICS_HOST_ARCH=windows-x64
set PATH=%PATH%;G:\epics\base\bin\%EPICS_HOST_ARCH%

REM ======================================================
REM   ====== OPTIONAL ENVIRONMENT VARIABLES FOLLOW =====
REM ======================================================

REM ======================================================
REM   --------------------- Git ------------------------
REM ======================================================
set PATH=%PATH%;C:\Program files\Git

REM ======================================================
REM   --------------- EPICS Extensions -----------------
REM ======================================================
REM set PATH=%PATH%;G:\epics\extensions\bin\%EPICS_HOST_ARCH%

REM ======================================================
REM   --------------- Exceed ---------------------------
REM ======================================================
REM    Needed for X11 extensions
REM set EX_VER=14.00
REM set EX_VER=15.00
REM set PATH=%PATH%;C:\Exceed%EX_VER%\XDK\
REM set PATH=%PATH%;C:\Program Files\Hummingbird\Connectivity\%EX_VER%\Exceed\

