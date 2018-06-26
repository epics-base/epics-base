#!/bin/sh
#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
#  Site-specific EPICS environment settings
#
#  sites should modify these definitions

# Location of epics base
if [ -z "${MY_EPICS_BASE}" ] ; then
	MY_EPICS_BASE=/usr/local/epics/base
fi

# Location of epics extensions (medm, msi, etc.)
if [ -z "${EPICS_EXTENSIONS}" ] ; then
	EPICS_EXTENSIONS=/usr/local/epics/extensions
fi

# Postscript printer definition needed by some extensions (eg medm, dp, dm, ...)
if [ -z "${PSPRINTER}" ] ; then
    export PSPRINTER=lp
fi

#Needed only by the idl and ezcaIDL extensions.
#export EPICS_EXTENSIONS

# Needed only by medm extension
#export EPICS_DISPLAY_PATH=/path/to/adl/files
export BROWSER=firefox

# Needed only by orbitscreen extension
#if [ -z "${ORBITSCREENHOME}" ] ; then
#	export "ORBITSCREENHOME=${EPICS_EXTENSIONS/src/orbitscreen}"
#fi

# Needed only by adt extension
#if [ -z "${ADTHOME}" ] ; then
#	ADTHOME=
#	export ADTHOME
#fi

# Needed only by ar extension (archiver)
#EPICS_AR_PORT=7002
#export EPICS_AR_PORT

# Needed for java extensions
if [ -z "${CLASSPATH}" ] ; then
	CLASSPATH="${EPICS_EXTENSIONS}/javalib"
else
	CLASSPATH="${CLASSPATH}:${EPICS_EXTENSIONS}/javalib"
fi
export CLASSPATH

# Allow private versions of extensions without a bin subdir
if [ -n "${EPICS_EXTENSIONS_PVT}" ] ; then
	PATH="${PATH}:${EPICS_EXTENSIONS_PVT}"
fi

#---------------------------------------------------------------
# Start of set R3.14 environment variables
#
EPICS_HOST_ARCH=`"${MY_EPICS_BASE}"/startup/EpicsHostArch.pl`
export EPICS_HOST_ARCH

# Allow private versions of base
if [ -n "${EPICS_BASE_PVT}" ] ; then
	if [ -d "${EPICS_BASE_PVT}/bin/${EPICS_HOST_ARCH}" ]; then
		PATH="${PATH}:${EPICS_BASE_PVT}/bin/${EPICS_HOST_ARCH}"
	fi
fi

# Allow private versions of extensions
if [ -n "${EPICS_EXTENSIONS_PVT}" ] ; then
	if [ -d "${EPICS_EXTENSIONS_PVT}/bin/${EPICS_HOST_ARCH}" ]; then
		PATH="${PATH}:${EPICS_EXTENSIONS_PVT}/bin/${EPICS_HOST_ARCH}"
	fi
fi
PATH="${PATH}:${EPICS_EXTENSIONS}/bin/${EPICS_HOST_ARCH}"

# End of set R3.14 environment variables

#---------------------------------------------------------------
#
## Start of set pre R3.14 environment variables
#
## Time service:
## EPICS_TS_MIN_WEST the local time difference from GMT.
#EPICS_TS_MIN_WEST=360
#export EPICS_TS_MIN_WEST
#
#HOST_ARCH=`"${MY_EPICS_BASE}"/startup/HostArch`
#export HOST_ARCH
#
## Allow private versions of base
#if [ -n "${EPICS_BASE_PVT}" ] ; then
#	if [ -d "${EPICS_BASE_PVT}/bin/${HOST_ARCH}" ]; then
#		PATH="${PATH}:${EPICS_BASE_PVT}/bin/${HOST_ARCH}"
#	fi
#fi
#
## Allow private versions of extensions
#if [ -n "${EPICS_EXTENSIONS_PVT}" ] ; then
#	if [ -d "${EPICS_EXTENSIONS_PVT}/bin/${HOST_ARCH}" ]; then
#		PATH="${PATH}:${EPICS_EXTENSIONS_PVT}/bin/${HOST_ARCH}"
#	fi
#fi
#
#PATH="${PATH}:${EPICS_EXTENSIONS}/lib/${HOST_ARCH}"
#
# End of set pre R3.14 environment variables

#---------------------------------------------------------------
