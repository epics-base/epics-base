#!/bin/csh -f
#  Site-specific EPICS environment settings
#
#  sites should modify these definitions

# Location of epics base
if ( ! $?EPICS_BASE ) then
	setenv EPICS_BASE /usr/local/epics/base
endif

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

# Needed for java extensions
if ( $?CLASSPATH ) then
   setenv CLASSPATH "${CLASSPATH}:${EPICS_EXTENSIONS}/javalib"
else
   setenv CLASSPATH "${EPICS_EXTENSIONS}/javalib"
endif

# Allow private versions of extensions without a bin subdir
if ( $?EPICS_EXTENSIONS_PVT ) then
    set path = ( $path $EPICS_EXTENSIONS_PVT)
endif

##################################################################
# Start of R3.13 environment variables

# Time service:
# EPICS_TS_MIN_WEST the local time difference from GMT.
setenv EPICS_TS_MIN_WEST 360

if ( -e /usr/local/etc/setup/HostArch.pl ) then
   setenv HOST_ARCH `/usr/local/etc/setup/HostArch.pl`
else
   setenv HOST_ARCH `/usr/local/epics/startup/HostArch.pl`
endif

# Allow private versions of base
if ( $?EPICS_BASE_PVT ) then
   if ( -e $EPICS_BASE_PVT/bin/$HOST_ARCH ) then
      set path = ( $path $EPICS_BASE_PVT/bin/$HOST_ARCH)
   endif
   if ( -e $EPICS_BASE_PVT/lib/$HOST_ARCH ) then
      if ( $?LD_LIBRARY_PATH ) then
         setenv LD_LIBRARY_PATH "${LD_LIBRARY_PATH}:${EPICS_BASE_PVT}/lib/${HOST_ARCH}"
      else
         setenv LD_LIBRARY_PATH "${EPICS_BASE_PVT}/lib/${HOST_ARCH}"
      endif
   endif
endif

set path = ( $path $EPICS_BASE/bin/$HOST_ARCH )

# Allow private versions of extensions
if ( $?EPICS_EXTENSIONS_PVT ) then
   if ( -e $EPICS_EXTENSIONS_PVT/bin/$HOST_ARCH ) then
      set path = ( $path $EPICS_EXTENSIONS_PVT/bin/$HOST_ARCH)
   endif
   # Needed if shared extension libraries are built
   if ( -e $EPICS_EXTENSIONS_PVT/lib/$HOST_ARCH ) then
      if ( $?LD_LIBRARY_PATH ) then
         setenv LD_LIBRARY_PATH "${LD_LIBRARY_PATH}:${EPICS_EXTENSIONS_PVT}/lib/${HOST_ARCH}"
      else
         setenv LD_LIBRARY_PATH "${EPICS_EXTENSIONS_PVT}/lib/${HOST_ARCH}"
      endif
   endif
endif

set path = ( $path $EPICS_EXTENSIONS/bin/$HOST_ARCH )

# Needed if shared base libraries are built
if ( $?LD_LIBRARY_PATH ) then
   setenv LD_LIBRARY_PATH "${LD_LIBRARY_PATH}:${EPICS_BASE}/lib/${EPICS_HOST_ARCH}"
else
   setenv LD_LIBRARY_PATH "${EPICS_BASE}/lib/${EPICS_HOST_ARCH}"
endif

# Needed if shared extension libraries are built
if ( $?LD_LIBRARY_PATH ) then
   setenv LD_LIBRARY_PATH "${LD_LIBRARY_PATH}:${EPICS_EXTENSIONS}/lib/${EPICS_HOST_ARCH}"
else
   setenv LD_LIBRARY_PATH "${EPICS_EXTENSIONS}/lib/${EPICS_HOST_ARCH}"
endif

# End of R3.13 environment variables
##################################################################
