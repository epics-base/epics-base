
TOP=../..

include $(TOP)/configure/CONFIG

INC := cvtTable.h

SRCS := makeBpt.c
PROD := makeBpt

MENUS += menuConvert.h

BPTS += bptTypeJdegC.dbd
BPTS += bptTypeJdegF.dbd
BPTS += bptTypeKdegC.dbd
BPTS += bptTypeKdegF.dbd

bptLibrary_SRCS = cvtBpt.c

IOC_LIBRARY_vxWorks = bptLibrary

include $(TOP)/configure/RULES_BUILD

$(BPTS): $(EPICS_BASE_HOST_BIN)/makeBpt$(EXE)

