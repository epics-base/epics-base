# Pre R3.14 statments may be uncommented if needed

#----------- Start of pre R3.12 path requirements -----------------------------
#
#PATH=$PATH:./.epicsUnix/$HOST_ARCH/bin:$HOME/op/$HOST_ARCH
#
#if [ -z "${EPICS_ADD_ON}" ]
#then
#	EPICS_ADD_ON=/usr/local/epics/add_on ; export EPICS_ADD_ON
#fi
#
#if [ -z "${PVT_ADD_ON}" ]
#then
#	PVT_ADD_ON=$HOME/bin ; export PVT_ADD_ON
#fi
#
#PATH=$PVT_ADD_ON:$EPICS_ADD_ON/bin:$PATH
#
##----------- End of pre R3.12 path requirements -------------------------------

#----------- Start of sun4 to solaris transitional path requirements ----------
#if [ ${HOST_ARCH}="solaris" ] ; then
#if [ -z "${EPICS_EXTENSIONS}" ] ; then
#       EPICS_EXTENSIONS=/usr/local/epics/extensions ; export EPICS_EXTENSIONS
#fi
#       PATH=${EPICS_EXTENSIONS}/bin/sun4:$PATH
#fi
##----------- End of sun4 to solaris transitional path requirements ------------
#
##----------- Start of R3.12 path requirements ---------------------------------
#
#PATH=./appSR/tools:./appSR/bin/$HOST_ARCH:./base/tools:./base/bin/$HOST_ARCH:$PATH
#
#----------- Start of R3.13 path requirements ---------------------------------
#if [ -z "${EPICS_EXTENSIONS}" ]
#then
#	EPICS_EXTENSIONS=/usr/local/epics/extensions ; export EPICS_EXTENSIONS
#fi
#
#if [ -z "${EPICS_EXTENSIONS_PVT}" ]
#then
#	PATH=${EPICS_EXTENSIONS}/bin/${HOST_ARCH}:$PATH
#else
#	PATH=${EPICS_EXTENSIONS_PVT}:${EPICS_EXTENSIONS}/bin/${HOST_ARCH}:$PATH
#fi
#
#export PATH
#----------- End of R3.13 path requirements ---------------------------------

#----------- End of R3.12 path requirements -----------------------------------

#----------- Start of R3.14 path requirements ---------------------------------
if [ -z "${EPICS_EXTENSIONS}" ]
then
	EPICS_EXTENSIONS=/usr/local/epics/extensions ; export EPICS_EXTENSIONS
fi

if [ -z "${EPICS_EXTENSIONS_PVT}" ]
then
	PATH=${EPICS_EXTENSIONS}/bin/${EPICS_HOST_ARCH}:$PATH
else
	PATH=${EPICS_EXTENSIONS_PVT}:${EPICS_EXTENSIONS}/bin/${EPICS_HOST_ARCH}:$PATH
fi

export PATH
#----------- End of R3.14 path requirements ---------------------------------
