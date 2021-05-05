@echo off
rem *************************************************************************
rem  Copyright (c) 2017 UChicago Argonne LLC, as Operator of Argonne
rem      National Laboratory.
rem  Copyright (c) 2002 The Regents of the University of California, as
rem      Operator of Los Alamos National Laboratory.
rem  SPDX-License-Identifier: EPICS
rem  EPICS BASE is distributed subject to a Software License Agreement found
rem  in file LICENSE that is included with this distribution.
rem *************************************************************************
rem
rem Site-specific EPICS environment settings
rem
rem Sets EPICS_HOST_ARCH and the environment for Microsoft Visual Studio.
rem Optionally, resets PATH, adds Strawberry Perl to PATH, and adds the
rem EPICS Base install host architecture bin directory to PATH.
rem

rem ----------------------------------------------------------------------
rem Site serviceable parts (These definitions may be modified)
rem ----------------------------------------------------------------------

rem The values of the definitions in this section must not contain
rem double-quotes.
rem
rem * Right: set _foo=C:\foo
rem * Right: set "_foo=C:\foo"
rem * Wrong: set _foo="C:\foo"

rem Automatically set up the environment when possible ("yes" or "no").
rem If set to yes, as much of the environment will be set up as possible.
rem If set to no, just the minimum environment will be set up.  More
rem specific _auto_* definitions take precedence over this definition.
set _auto=no

rem Automatically reset PATH ("yes" or "no").  If set to yes, PATH will
rem be reset to the value of _path_new.  If set to no, PATH will not be
rem reset.
set _auto_path_reset=%_auto%

rem Automatically append to PATH ("yes" or "no").  If set to yes, the
rem EPICS Base install host architecture bin directory will be added to
rem PATH if possible.  If set to no, the bin directory will not be added
rem to PATH.
set _auto_path_append=%_auto%

rem The new value for PATH.  If _auto_path_reset is yes, PATH will be set
rem to it.
set _path_new=C:\Windows\System32;C:\Windows;C:\Windows\System32\wbem

rem The location of Strawberry Perl (pathname).  If empty, Strawberry Perl
rem is assumed to already be in PATH and will not be added.  If nonempty,
rem Strawberry Perl will be added to PATH.
set _strawberry_perl_home=C:\Strawberry

rem The location of Microsoft Visual Studio (pathname).
set _visual_studio_home=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community

rem The EPICS host architecture specification for EPICS_HOST_ARCH
rem (<os>-<arch>[-<toolset>] as defined in configure/CONFIG_SITE).
set _epics_host_arch=windows-x64

rem The install location of EPICS Base (pathname).  If nonempty and
rem _auto_path_append is yes, it will be used to add the host architecture
rem bin directory to PATH.
set _epics_base=

rem ----------------------------------------------------------------------
rem Internal parts (There is typically no need to modify these)
rem ----------------------------------------------------------------------

rem Reset PATH
if "%_auto_path_reset%" == "yes" (
  set "PATH=%_path_new%"
)

rem Add Strawberry Perl to PATH
if "%_strawberry_perl_home%" == "" goto after_add_strawberry_perl
rem Can't do this inside parentheses because PATH would be read only once
set "PATH=%PATH%;%_strawberry_perl_home%\c\bin"
set "PATH=%PATH%;%_strawberry_perl_home%\perl\site\bin"
set "PATH=%PATH%;%_strawberry_perl_home%\perl\bin"
:after_add_strawberry_perl

rem Set the environment for Microsoft Visual Studio
call "%_visual_studio_home%\VC\Auxiliary\Build\vcvarsall.bat" x64

rem Set the EPICS host architecture specification
set "EPICS_HOST_ARCH=%_epics_host_arch%"

rem Add the EPICS Base host architecture bin directory to PATH
if "%_auto_path_append%" == "yes" (
  if not "%_epics_base%" == "" (
    set "PATH=%PATH%;%_epics_base%\bin\%_epics_host_arch%"
  )
)

rem Don't leak variables into the environment
set _auto=
set _auto_path_reset=
set _auto_path_append=
set _path_new=
set _strawberry_perl_home=
set _visual_studio_home=
set _epics_host_arch=
set _epics_base=
