TOP=../..
include $(TOP)/configure/CONFIG

INC += cvtTable.h

MENUS += menuConvert.h

BPTS += bptTypeJdegC.dbd
BPTS += bptTypeJdegF.dbd
BPTS += bptTypeKdegC.dbd
BPTS += bptTypeKdegF.dbd

PROD_LIBS = dbStaticHost Com
PROD += makeBpt

include $(TOP)/configure/RULES

$(INSTALL_BPTS): $(INSTALL_PROD)
