#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# SPDX-License-Identifier: EPICS
# EPICS Base is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************
#
# Site-specific EPICS environment settings
#
# Attempts to set EPICS_HOST_ARCH.  Optionally, adds the EPICS Base
# install host architecture bin directory to PATH.
#

#-----------------------------------------------------------------------
# Site serviceable parts (These definitions may be modified)
#-----------------------------------------------------------------------

# Automatically set up the environment when possible ("yes" or "no").
# If set to yes, as much of the environment will be set up as possible.
# If set to no, just the minimum environment will be set up.  More
# specific _auto_* definitions take precedence over this definition.
set _auto=no

# Automatically append to PATH ("yes" or "no").  If set to yes, the
# EPICS Base install host architecture bin directory will be added to
# PATH if possible.  If set to no, the bin directory will not be added
# to PATH.
set _auto_path_append=$_auto

# The program used to run Perl scripts (pathname).
set _perl_prog=perl

# The EPICS host architecture specification for EPICS_HOST_ARCH
# (<os>-<arch>[-<toolset>] as defined in configure/CONFIG_SITE).  If
# nonempty, the value will be used as the value of EPICS_HOST_ARCH.  If
# empty, an attempt will be made to automatically determine the value
# with EpicsHostArch.pl.
set _epics_host_arch=

# The install location of EPICS Base (pathname).  If nonempty, the
# EpicsHostArch.pl script from it, if it exists, will be used to
# determine EPICS_HOST_ARCH.  If nonempty and EPICS_HOST_ARCH was
# determined successfully, it will be used to add the host architecture
# bin directory to PATH if _auto_path_append is yes.
set _epics_base=

# The source location of EPICS Base (pathname).  If nonempty, the
# EpicsHostArch.pl script from it, if it exists and _epics_base is empty
# or it did not exist in the _epics_base location, will be used to
# determine EPICS_HOST_ARCH.
set _epics_base_src=

#-----------------------------------------------------------------------
# Internal parts (There is typically no need to modify these)
#-----------------------------------------------------------------------

# Define the possible locations of EpicsHostArch.pl
set _epics_host_arch_pl=
set _src_epics_host_arch_pl=
if ("$_epics_base" != '') then
  set _epics_host_arch_pl="$_epics_base/lib/perl/EpicsHostArch.pl"
endif
if ("$_epics_base_src" != '') then
  set _src_epics_host_arch_pl="$_epics_base_src/src/tools/EpicsHostArch.pl"
endif

# Set the EPICS host architecture specification
if ("$_epics_host_arch" != '') then
  setenv EPICS_HOST_ARCH "$_epics_host_arch"
else if (-e "$_epics_host_arch_pl") then
  set _epics_host_arch=`"$_perl_prog" "$_epics_host_arch_pl"`
  setenv EPICS_HOST_ARCH "$_epics_host_arch"
else if (-e "$_src_epics_host_arch_pl") then
  set _epics_host_arch=`"$_perl_prog" "$_src_epics_host_arch_pl"`
  setenv EPICS_HOST_ARCH "$_epics_host_arch"
endif

# Add the EPICS Base host architecture bin directory to PATH
if ("$_auto_path_append" == yes) then
  if ("$_epics_base" != '' && "$_epics_host_arch" != '') then
    setenv PATH "${PATH}:$_epics_base/bin/$_epics_host_arch"
  endif
endif

# Don't leak variables into the environment
unset _auto
unset _auto_path_append
unset _perl_prog
unset _epics_host_arch
unset _epics_base
unset _epics_base_src
unset _epics_host_arch_pl
unset _src_epics_host_arch_pl
