TOP=../../..

include $(TOP)/configure/CONFIG

LIBRARY_vxWorks = iocCore

LIBOBJS = \
	$(INSTALL_BIN)/ComLibrary \
	$(INSTALL_BIN)/asLibrary \
	$(INSTALL_BIN)/bptLibrary \
	$(INSTALL_BIN)/dbLibrary \
	$(INSTALL_BIN)/dbStaticLibrary \
	$(INSTALL_BIN)/dbSubsLibrary \
	$(INSTALL_BIN)/caLibrary \
	$(INSTALL_BIN)/miscLibrary \
	$(INSTALL_BIN)/registryLibrary \
	$(INSTALL_BIN)/rsrvLibrary \
	$(INSTALL_BIN)/vxComLibrary \
	$(INSTALL_BIN)/vxDbLibrary

include $(TOP)/configure/RULES

iocCore: $(LIBOBJS)
