TOP=../../..

include $(TOP)/configure/CONFIG

PROD_LIBS += dbIoc dbStaticHost Com
dbIoc_DIR=$(INSTALL_LIB)
dbStaticHost_DIR=$(INSTALL_LIB)

callbackTestHost_SRCS += callbackTestMain.c callbackTest.c
PROD += callbackTestHost
OBJS_IOC += callbackTest


include $(TOP)/configure/RULES

