#!/bin/sh
#  Site-specific EPICS environment settings

# Start of set pre R3.14 environment variable
#HOST_ARCH=`/usr/local/epics/startup/HostArch`
#export HOST_ARCH
# End of set pre R3.14 environment variable

# Start of set R3.14 environment variable
EPICS_HOST_ARCH=`/usr/local/epics/startup/EpicsHostArch`
export EPICS_HOST_ARCH
# End of set R3.14 environment variable

# Time service:
# EPICS_TS_MIN_WEST the local time difference from GMT.
# EPICS_TS_NTP_INET ntp or Unix time server ip addr.
EPICS_TS_MIN_WEST=360
export EPICS_TS_MIN_WEST

# Postscript printer definition needed by some extensions (eg medm, dp, dm, ...)
PSPRINTER=lp
export PSPRINTER

# Needed only by medm extension
#setenv EPICS_DISPLAY_PATH
#export EPICS_DISPLAY_PATH

# Needed only by orbitscreen extension
#if [ $ORBITSCREENHOME="" ] ; then
#        #setenv ORBITSCREENHOME $EPICS_ADD_ON/src/orbitscreen
#        ORBITSCREENHOME=$EPICS_EXTENSIONS/src/orbitscreen
#	export ORBITSCREENHOME
#fi

# Needed only by ar extension (archiver)
#EPICS_AR_PORT=7002
#export EPICS_AR_PORT


. /usr/local/epics/startup/.profile


