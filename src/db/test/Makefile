TOP=../../..

include $(TOP)/configure/CONFIG

PROD_LIBS += db dbStaticHost Com
#PROD_LIBS += db ca dbStaticHost Com

callbackTestHost_SRCS += callbackTestMain.c callbackTest.c
PROD += callbackTestHost
OBJS_IOC += callbackTest


include $(TOP)/configure/RULES

