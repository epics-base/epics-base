TOP=../../..

include $(TOP)/configure/CONFIG

LIBRARY_vxWorks = iocCore

LIBOBJS = \
	$(INSTALL_BIN)/asLibrary \
	$(INSTALL_BIN)/caLib \
	$(INSTALL_BIN)/dbLib \
	$(INSTALL_BIN)/libCom \
	$(INSTALL_BIN)/dbStaticLib \
	$(INSTALL_BIN)/miscLib \
	$(INSTALL_BIN)/bptLibrary \
	$(INSTALL_BIN)/registryLibrary \
	$(INSTALL_BIN)/rsrvLib \
	$(INSTALL_BIN)/vxComLib \
	$(INSTALL_BIN)/vxDbLib

include $(TOP)/configure/RULES

iocCore: $(LIBOBJS)
