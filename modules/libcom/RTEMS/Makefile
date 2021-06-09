#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

TOP = ../../..
include $(TOP)/configure/CONFIG
include $(TOP)/configure/CONFIG_LIBCOM_VERSION

# check check
SRC_DIRS += ../$(OS_API)

PERL_SCRIPTS += epicsMakeMemFs.pl

INC += epicsRtemsInitHooks.h
INC += epicsMemFs.h
INC += epicsNtp.h

ifeq ($(RTEMS_QEMU_FIXUPS),YES)
rtems_init_CPPFLAGS += -DQEMU_FIXUPS
endif
rtemsCom_SRCS += rtems_init.c
rtemsCom_SRCS += rtems_config.c
rtemsCom_SRCS += rtems_netconfig.c
rtemsCom_SRCS += rtems_util.c
rtemsCom_SRCS += setBootConfigFromNVRAM.c
rtemsCom_SRCS += epicsRtemsInitHookPre.c
rtemsCom_SRCS += epicsRtemsInitHookPost.c
rtemsCom_SRCS += epicsMemFs.c
rtemsCom_SRCS += epicsNtp.c

ifeq ($(RTEMS_TARGET_CPU),i386)
rtemsCom_SRCS += ne2kpci.c
endif

rtems_config_CPPFLAGS += -DBSP_$(RTEMS_BSP)

LIBRARY_RTEMS = rtemsCom

# shared library ABI version.
SHRLIB_VERSION = $(EPICS_LIBCOM_MAJOR_VERSION).$(EPICS_LIBCOM_MINOR_VERSION).$(EPICS_LIBCOM_MAINTENANCE_VERSION)

include $(TOP)/configure/RULES
