#!/bin/csh -f
#  Site-specific EPICS environment settings
#
#  sites should modify these definitions

# Start of set pre R3.14 environment variable
#if ( -e /usr/local/etc/setup/HostArch ) then
#   setenv HOST_ARCH `/usr/local/etc/setup/HostArch`
#else
#   setenv HOST_ARCH `/usr/local/epics/startup/HostArch`
#endif
# End of set pre R3.14 environment variable

# Start of set R3.14 environment variable
if ( -e /usr/local/etc/setup/EpicsHostArch ) then
   setenv EPICS_HOST_ARCH `/usr/local/etc/setup/EpicsHostArch`
else
   setenv EPICS_HOST_ARCH `/usr/local/epics/startup/EpicsHostArch`
endif
# End of set R3.14 environment variable


# Time service:
# EPICS_TS_MIN_WEST the local time difference from GMT.
# EPICS_TS_NTP_INET ntp or Unix time server ip addr.
setenv EPICS_TS_MIN_WEST 360

# Needed only by ar extension (archiver)
#setenv EPICS_AR_PORT 7002

# Postscript printer definition needed by some extensions (eg medm, dp, dm, ...)
#setenv PSPRINTER lp

# Needed only by medm extension
#setenv EPICS_DISPLAY_PATH

if ( -e /usr/local/etc/setup/.cshrc ) then
   source /usr/local/etc/setup/.cshrc
else
   source /usr/local/epics/startup/.cshrc
endif


