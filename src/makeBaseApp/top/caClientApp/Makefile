TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

PROD_HOST += caExample
caExample_SRCS	+= caExample.c
caExample_LIBS	+= ca
caExample_LIBS	+= Com

PROD_HOST += caMonitor
caMonitor_SRCS	+= caMonitor.c
caMonitor_LIBS	+= ca
caMonitor_LIBS	+= Com

ca_DIR           = $(EPICS_BASE_LIB)
Com_DIR          = $(EPICS_BASE_LIB)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

