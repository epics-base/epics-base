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
_auto=no

# Automatically append to PATH ("yes" or "no").  If set to yes, the
# EPICS Base install host architecture bin directory will be added to
# PATH if possible.  If set to no, the bin directory will not be added
# to PATH.
_auto_path_append=$_auto

# The program used to run Perl scripts (pathname).
_perl_prog=perl

# The EPICS host architecture specification for EPICS_HOST_ARCH
# (<os>-<arch>[-<toolset>] as defined in configure/CONFIG_SITE).  If
# nonempty, the value will be used as the value of EPICS_HOST_ARCH.  If
# empty, an attempt will be made to automatically determine the value
# with EpicsHostArch.pl.
_epics_host_arch=

# The install location of EPICS Base (pathname).  If nonempty, the
# EpicsHostArch.pl script from it, if it exists, will be used to
# determine EPICS_HOST_ARCH.  If nonempty and EPICS_HOST_ARCH was
# determined successfully, it will be used to add the host architecture
# bin directory to PATH if _auto_path_append is yes.
_epics_base=

# The source location of EPICS Base (pathname).  If nonempty, the
# EpicsHostArch.pl script from it, if it exists and _epics_base is empty
# or it did not exist in the _epics_base location, will be used to
# determine EPICS_HOST_ARCH.
_epics_base_src=

#-----------------------------------------------------------------------
# Internal parts (There is typically no need to modify these)
#-----------------------------------------------------------------------

# Define the possible locations of EpicsHostArch.pl
_epics_host_arch_pl=
_src_epics_host_arch_pl=
if [ -n "$_epics_base" ]; then
  _epics_host_arch_pl="$_epics_base/lib/perl/EpicsHostArch.pl"
fi
if [ -n "$_epics_base_src" ]; then
  _src_epics_host_arch_pl="$_epics_base_src/src/tools/EpicsHostArch.pl"
fi

# Set the EPICS host architecture specification
if [ -n "$_epics_host_arch" ]; then
  EPICS_HOST_ARCH=$_epics_host_arch
  export EPICS_HOST_ARCH
elif [ -e "$_epics_host_arch_pl" ]; then
  _epics_host_arch=$("$_perl_prog" "$_epics_host_arch_pl")
  EPICS_HOST_ARCH=$_epics_host_arch
  export EPICS_HOST_ARCH
elif [ -e "$_src_epics_host_arch_pl" ]; then
  _epics_host_arch=$("$_perl_prog" "$_src_epics_host_arch_pl")
  EPICS_HOST_ARCH=$_epics_host_arch
  export EPICS_HOST_ARCH
fi

# Add the EPICS Base host architecture bin directory to PATH
if [ "$_auto_path_append" = yes ]; then
  if [ -n "$_epics_base" ] && [ -n "$_epics_host_arch" ]; then
    PATH="$PATH:$_epics_base/bin/$_epics_host_arch"
    export PATH
  fi
fi

# Don't leak variables into the environment
unset _auto
unset _auto_path_append
unset _perl_prog
unset _epics_host_arch
unset _epics_base
unset _epics_base_src
unset _epics_host_arch_pl
unset _src_epics_host_arch_pl
