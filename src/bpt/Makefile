TOP=../..
include $(TOP)/configure/CONFIG

INC += cvtTable.h

makeBpt_SRCS += makeBpt.c
PROD += makeBpt

MENUS += menuConvert.h

bpt_SRCS += cvtBpt.c

LIBRARY_IOC = bpt
DLL_LIBS = Db Com

BPTS += bptTypeJdegC.dbd
BPTS += bptTypeJdegF.dbd
BPTS += bptTypeKdegC.dbd
BPTS += bptTypeKdegF.dbd

include $(TOP)/configure/RULES

$(BPTS): $(INSTALL_PROD)
