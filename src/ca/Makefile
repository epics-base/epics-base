
TOP=../..

include $(TOP)/configure/CONFIG

CMPLR = STRICT

#
#	includes to install from this subproject
#
#INC += cadef.h moved header to src/include
#INC += caerr.h moved header to src/include
#INC += caeventmask.h moved header to src/include
INC += caProto.h

#
#	Contents of library: generic, special and for all systems
#

# on generic system
ca_SRCS_DEFAULT = posix_depen.c if_depen.c

# on WIN32 only
ca_SRCS_WIN32   = windows_depen.c

# on all systems
ca_SRCS += iocinf.c
ca_SRCS += service.c
ca_SRCS += conn.c
ca_SRCS += flow_control.c
ca_SRCS += syncgrp.c
ca_SRCS += access.c
ca_SRCS += convert.c
ca_SRCS += test_event.c
ca_SRCS += bsd_depen.c
ca_SRCS += repeater.c

#       Library to build:
#       lib$(LIBRARY).a  or   ..dll/..exp/..lib
#
LIBRARY_DEFAULT = ca

PROD_DEFAULT := caRepeater
PROD_vxWorks := -nil-

#	libs needed to build [TEST]PRODUCT
PROD_LIBS = ca Com
ca_DIR = .
Com_DIR = $(INSTALL_LIB)

SYS_PROD_LIBS_WIN32 = ws2_32 user32 advapi32

TESTPROD_DEFAULT := catime acctst 
TESTPROD_vxWorks :=

caLib_SRCS = \
	iocinf.o access.c test_event.c service.c flow_control.c \
	repeater.c conn.c syncgrp.c if_depen.c bsd_depen.c vxWorks_depen.c \
	convert.c

LIBRARY_vxWorks = caLib 

include $(TOP)/configure/RULES_BUILD

pcatime : catime.o $(PRODDEPLIBS)
	$(PURIFY) $(LINK.c) catime.o $(LDLIBS)

pacctst : acctst.o $(PRODDEPLIBS)
	$(PURIFY) $(LINK.c) acctst.o $(LDLIBS)

#	EOF Makefile.Host
