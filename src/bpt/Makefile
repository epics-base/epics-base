TOP=../..
include $(TOP)/configure/CONFIG

INC += cvtTable.h

makeBpt_SRCS += makeBpt.c
PROD += makeBpt

MENUS += menuConvert.h

bptLibrary_SRCS += cvtBpt.c

LIBRARY_IOC = bptLibrary
DLL_LIBS = Db Com

BPTS += bptTypeJdegC.dbd
BPTS += bptTypeJdegF.dbd
BPTS += bptTypeKdegC.dbd
BPTS += bptTypeKdegF.dbd

include $(TOP)/configure/RULES

$(BPTS): $(INSTALL_PROD)
