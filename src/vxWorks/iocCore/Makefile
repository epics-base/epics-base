TOP=../../..

include $(TOP)/configure/CONFIG

iocCore_OBJS += $(INSTALL_BIN)/ComLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/caLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/dbStaticIocLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/registryIocLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/dbIocLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/asIocLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/miscIocLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/dbtoolsIocLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/rsrvIocLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/vxComLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/vxDbLibrary.o
iocCore_OBJS += $(INSTALL_BIN)/iocshLibrary.o
iocCore_OBJS += registerRecordDeviceDriver.o

OBJLIB_vxWorks = iocCore
OBJLIB_OBJS += $(iocCore_OBJS)

include $(TOP)/configure/RULES

