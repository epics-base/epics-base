

#----------- Start of R3.14 epics extensions path requirements --------------------------------------
if ( ! $?EPICS_EXTENSIONS ) then
	setenv EPICS_EXTENSIONS /usr/local/epics/extensions
endif
if ( $?EPICS_EXTENSIONS_PVT ) then
	set path = ( $path $EPICS_EXTENSIONS_PVT $EPICS_EXTENSIONS/bin/$EPICS_HOST_ARCH)
	if ( $?LD_LIBRARY_PATH ) then
		setenv LD_LIBRARY_PATH $LD_LIBRARY_PATH:$EPICS_EXTENSIONS_PVT/lib/$EPICS_HOST_ARCH):$EPICS_EXTENSIONS/lib/$EPICS_HOST_ARCH)
	else
		setenv LD_LIBRARY_PATH $EPICS_EXTENSIONS_PVT/lib/$EPICS_HOST_ARCH):$EPICS_EXTENSIONS/lib/$EPICS_HOST_ARCH)
	endif
else
	set path = ( $path $EPICS_EXTENSIONS/bin/$EPICS_HOST_ARCH)
	if ( $?LD_LIBRARY_PATH ) then
		setenv LD_LIBRARY_PATH $LD_LIBRARY_PATH:$EPICS_EXTENSIONS/lib/$EPICS_HOST_ARCH)
	else
		setenv LD_LIBRARY_PATH $EPICS_EXTENSIONS/lib/$EPICS_HOST_ARCH)
	endif
endif
#----------- End of R3.14 epics extensions path requirements -----------------------------------------

#----------- Start of R3.13 epics extensions path requirements --------------------------------------
if ( ! $?EPICS_EXTENSIONS ) then
	setenv EPICS_EXTENSIONS /usr/local/epics/extensions
endif
if ( $?EPICS_EXTENSIONS_PVT ) then
	set path = ( $path $EPICS_EXTENSIONS_PVT $EPICS_EXTENSIONS/bin/$HOST_ARCH)
	if ( $?LD_LIBRARY_PATH ) then
		setenv LD_LIBRARY_PATH $LD_LIBRARY_PATH:$EPICS_EXTENSIONS_PVT/lib/$HOST_ARCH):$EPICS_EXTENSIONS/lib/$HOST_ARCH)
	else
		setenv LD_LIBRARY_PATH $EPICS_EXTENSIONS_PVT/lib/$HOST_ARCH):$EPICS_EXTENSIONS/lib/$HOST_ARCH)
	endif
else
	set path = ( $path $EPICS_EXTENSIONS/bin/$HOST_ARCH)
	if ( $?LD_LIBRARY_PATH ) then
		setenv LD_LIBRARY_PATH $LD_LIBRARY_PATH:$EPICS_EXTENSIONS/lib/$HOST_ARCH)
	else
		setenv LD_LIBRARY_PATH $EPICS_EXTENSIONS/lib/$HOST_ARCH)
	endif
endif
#----------- End of R3.13 epics extensions path requirements -----------------------------------------
