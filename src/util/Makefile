TOP=../..

include $(TOP)/configure/CONFIG

PROD_LIBS  = ca Com

#
# Added winmm user32 for the non-dll build
#
PROD_HOST_DEFAULT = ca_test iocLogServer startCArepeater 
PROD_HOST_WIN32   = ca_test iocLogServer
SYS_PROD_LIBS_WIN32 = ws2_32 advapi32 user32

ca_test_SRCS = ca_test_main.c ca_test.c
iocLogServer_SRCS = iocLogServer.c
startCArepeater_SRCS = startCArepeater.c

OBJS = ca_test

SCRIPTS_solaris := S99logServer S99caRepeater 

include $(TOP)/configure/RULES
 
S99%: ../rc2.%
	@/usr/bin/sed -e s%:EPICS:%`chdir $(TOP); /usr/bin/pwd`% \
		-e s%:INSTALL_BIN:%`chdir $(INSTALL_BIN); /usr/bin/pwd`% \
		$< >$@

#	EOF Makefile.Host for base/src/util
