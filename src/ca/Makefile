
TOP=../..

include $(TOP)/configure/CONFIG

CMPLR = STRICT

#
#	includes to install from this subproject
#
INC += cadef.h
INC += caerr.h
INC += caeventmask.h
INC += caProto.h

#
#	Contents of library: generic, special and for all systems
#

# on generic system
LIBSRCS_DEFAULT := posix_depen.c if_depen.c

# on WIN32 only
LIBSRCS_WIN32   := windows_depen.c

# on all systems
LIBSRCS += iocinf.c
LIBSRCS += service.c
LIBSRCS += conn.c
LIBSRCS += flow_control.c
LIBSRCS += syncgrp.c
LIBSRCS += access.c
LIBSRCS += convert.c
LIBSRCS += test_event.c
LIBSRCS += bsd_depen.c
LIBSRCS += repeater.c

#       Library to build:
#       lib$(LIBRARY).a  or   ..dll/..exp/..lib
#
LIBRARY := ca

#	build LIBRARY as a DLL on WIN32, other systems may ignore this
LIBTYPE := SHARED

PROD := caRepeater

#	libs needed to build [TEST]PRODUCT
PROD_LIBS := ca Com Osi
SYS_PROD_LIBS_WIN32 := ws2_32 user32 advapi32

TESTPROD := catime acctst 

caLib_SRCS = \
	iocinf.o access.c test_event.c service.c flow_control.c \
	repeater.c conn.c syncgrp.c if_depen.c bsd_depen.c vxWorks_depen.c \
	convert.c

IOC_LIBRARY_vxWorks = caLib 

include $(TOP)/configure/RULES_BUILD

pcatime : catime.o $(PRODDEPLIBS)
	$(PURIFY) $(LINK.c) catime.o $(LDLIBS)

pacctst : acctst.o $(PRODDEPLIBS)
	$(PURIFY) $(LINK.c) acctst.o $(LDLIBS)

#	EOF Makefile.Host

