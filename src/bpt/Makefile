TOP=../..
include $(TOP)/configure/CONFIG

INC += cvtTable.h

MENUS += menuConvert.h

BPTS += bptTypeJdegC.dbd
BPTS += bptTypeJdegF.dbd
BPTS += bptTypeKdegC.dbd
BPTS += bptTypeKdegF.dbd

LIBSRCS += cvtBpt.c
LIBRARY_IOC = bptIoc

bptHost_DLL_LIBS = dbStaticHost Com
bptIoc_DLL_LIBS = dbStaticIoc Com

PROD_LIBS = dbStaticHost Com
PROD += makeBpt


include $(TOP)/configure/RULES

$(BPTS): $(INSTALL_PROD)
