#!/bin/csh -f
#  Site-specific EPICS environment settings
#
#  sites should modify these definitions

# Location of epics extensions
if ( ! $?EPICS_EXTENSIONS ) then
	setenv EPICS_EXTENSIONS /usr/local/epics/extensions
endif

# Time service:
# EPICS_TS_NTP_INET ntp or Unix time server ip addr.

# Postscript printer definition needed by some extensions (eg medm, dp, dm, ...)
setenv PSPRINTER lp

# Needed only by medm extension
setenv EPICS_DISPLAY_PATH

# Needed only by orbitscreen extension
if ( ! $?ORBITSCREENHOME ) then
	setenv ORBITSCREENHOME $EPICS_EXTENSIONS/src/orbitscreen
endif

# Needed only by adt extension
if ( ! $?ADTHOME ) then
	setenv ADTHOME /usr/local/oag/apps/src/appconfig/adt
	echo $ADTHOME
endif

# Needed only by ar extension (archiver)
setenv EPICS_AR_PORT 7002
##################################################################

# Start of set R3.14 environment variables

if ( -e /usr/local/etc/setup/EpicsHostArch.pl ) then
   setenv EPICS_HOST_ARCH `/usr/local/etc/setup/EpicsHostArch`
else
   setenv EPICS_HOST_ARCH `/usr/local/epics/startup/EpicsHostArch`
endif

# Allow private versions of extensions
if ( $?EPICS_EXTENSIONS_PVT ) then
	set path = ( $path $EPICS_EXTENSIONS_PVT/bin/$EPICS_HOST_ARCH)
	# Needed if shared extension libraries are built
	if ( $?LD_LIBRARY_PATH ) then
		setenv LD_LIBRARY_PATH "${LD_LIBRARY_PATH}:${EPICS_EXTENSIONS_PVT}/lib/${EPICS_HOST_ARCH}"
	else
		setenv LD_LIBRARY_PATH "${EPICS_EXTENSIONS_PVT}/lib/${EPICS_HOST_ARCH}"
	endif
endif

set path = ( $path $EPICS_EXTENSIONS/bin/$EPICS_HOST_ARCH )
# Needed if shared extension libraries are built
if ( $?LD_LIBRARY_PATH ) then
   setenv LD_LIBRARY_PATH "${LD_LIBRARY_PATH}:${EPICS_EXTENSIONS}/lib/${EPICS_HOST_ARCH}"
else
   setenv LD_LIBRARY_PATH "${EPICS_EXTENSIONS}/lib/${EPICS_HOST_ARCH}"
endif

# End of set R3.14 environment variables
##################################################################


# Start of set pre R3.14 environment variables

# Time service:
# EPICS_TS_MIN_WEST the local time difference from GMT.
setenv EPICS_TS_MIN_WEST 360

if ( -e /usr/local/etc/setup/HostArch ) then
   setenv HOST_ARCH `/usr/local/etc/setup/HostArch`
else
   setenv HOST_ARCH `/usr/local/epics/startup/HostArch`
endif

# Allow private versions of extensions
if ( $?EPICS_EXTENSIONS_PVT ) then
	set path = ( $path $EPICS_EXTENSIONS_PVT/bin/$HOST_ARCH )
	# Needed if shared extension libraries are built
	setenv LD_LIBRARY_PATH "${LD_LIBRARY_PATH}:${EPICS_EXTENSIONS_PVT}/lib/${HOST_ARCH}"
endif

set path = ( $path $EPICS_EXTENSIONS/bin/$HOST_ARCH )
# Needed if shared extension libraries are built
setenv LD_LIBRARY_PATH "${LD_LIBRARY_PATH}:${EPICS_EXTENSIONS}/lib/${HOST_ARCH}"

# End of set pre R3.14 environment variables
##################################################################
