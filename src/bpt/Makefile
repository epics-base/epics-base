TOP=../..
include $(TOP)/configure/CONFIG

INC += cvtTable.h

DBD += menuConvert.dbd
INC += menuConvert.h

DBD += bptTypeJdegC.dbd
DBD += bptTypeJdegF.dbd
DBD += bptTypeKdegC.dbd
DBD += bptTypeKdegF.dbd

PROD_LIBS = dbStaticHost Com
PROD_HOST += makeBpt
makeBpt_SRCS=makeBpt

include $(TOP)/configure/RULES

$(COMMON_DBDS): $(INSTALL_PROD)

