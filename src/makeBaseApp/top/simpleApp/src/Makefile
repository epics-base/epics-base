TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#=======Create Host Programs=======
ca_DIR = $(EPICS_BASE_LIB)
Com_DIR = $(EPICS_BASE_LIB)

#USR_CFLAGS +=

# For Each Host Product define the following:1
#PROD_HOST = <product>
#<product>_SRCS += xxx.c 
#...
#<product>_LIBS += ca
#<product>_LIBS += Com

#========Database Products================

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord

#<name>.dbd will be created from <name>Include.dbd
#DBD += <name>.dbd

#=======Create an IOC program=====

#PROD_IOC = <name>

#<name>_SRCS += xxx.c # .cpp .stt etc
#...
#<name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
#<name>_SRCS += <name>_registerRecordDeviceDriver.cpp

## If <name> is to be built for both vxWorks and for the host then
#<name>_SRCS_DEFAULT += <name>Main.cpp
#<name>_SRCS_vxWorks += -nil-

#The following adds support from base/src/vxWorks
#<name>_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

##Define all possible libraries
## Use win32 object libs for registered support
#<name>_LIBS_win32 += recIocObj
#<name>_LIBS_win32 += softDevIocObj
#<name>_LIBS_win32 += testDevIocObj
#<name>_LIBS_DEFAULT += recIoc
#<name>_LIBS_DEFAULT += softDevIoc
#<name>_LIBS_DEFAULT += testDevIoc

#<name>_LIBS += iocsh
#<name>_LIBS += miscIoc
#<name>_LIBS += rsrvIoc
#<name>_LIBS += dbtoolsIoc
#<name>_LIBS += asIoc
#<name>_LIBS += dbIoc
#<name>_LIBS += registryIoc
#<name>_LIBS += dbStaticIoc
#<name>_LIBS += ca
#<name>_LIBS += Com

## If sequence programs are being built
#<name>_LIBS += seq
#<name>_LIBS += pv
#seq_DIR    = $(SNCSEQ_LIB)
#pv_DIR     = $(SNCSEQ_LIB)



include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

