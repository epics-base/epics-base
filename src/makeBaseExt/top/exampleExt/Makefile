TOP=../..
include $(TOP)/configure/CONFIG

PROD_HOST = caExample
caExample_SRCS += caExample.c
caExample_LIBS += $(EPICS_BASE_HOST_LIBS)

#caExample_CFLAGS +=    #C source file compiler flags, e.g. -DMYDEF=3
#caExample_LDFLAGS +=    #Linker flags, e.g.  -L$(MOTIF_LIB) -L$(X11_LIB)   
#caExample_SYS_LIBS_solaris += #System libraries for OS_CLASS solaris, e.g. socket

include $(TOP)/configure/RULES

