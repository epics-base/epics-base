TOP=../../..
include $(TOP)/configure/CONFIG

SRCS += rtems_init.c
SRCS += rtems_config.c
SRCS += rtems_netconfig.c
SRCS += rtems_util.c
SRCS += rtems_dummyreg.c

LIBRARY_RTEMS = rtemsCom
LIBRARY_SRCS = $(SRCS) $(BUILD_ARCHS)

include $(TOP)/configure/RULES
