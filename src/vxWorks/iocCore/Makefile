TOP=../../..

include $(TOP)/configure/CONFIG

LIBRARY_vxWorks = iocCore

iocCore_OBJS = \
	$(INSTALL_BIN)/ComLibrary.a \
	$(INSTALL_BIN)/asLibrary.a \
	$(INSTALL_BIN)/bptLibrary.a \
	$(INSTALL_BIN)/dbLibrary.a \
	$(INSTALL_BIN)/dbStaticLibrary.a \
	$(INSTALL_BIN)/dbSubsLibrary.a \
	$(INSTALL_BIN)/caLibrary.a \
	$(INSTALL_BIN)/miscLibrary.a \
	$(INSTALL_BIN)/registryLibrary.a \
	$(INSTALL_BIN)/rsrvLibrary.a \
	$(INSTALL_BIN)/vxComLibrary.a \
	$(INSTALL_BIN)/vxDbLibrary.a

include $(TOP)/configure/RULES

iocCore: $(LIBOBJS)
