
TOP=../../../..

include $(TOP)/configure/CONFIG

CXXCMPLR = STRICT

PROD_LIBS := cas ca gdd Com
cas_DIR = $(INSTALL_LIB)
ca_DIR = $(INSTALL_LIB)
gdd_DIR = $(INSTALL_LIB)
Com_DIR = $(INSTALL_LIB)

#
# Added ws2_32 winmm user32 for the non-dll build
#
SYS_PROD_LIBS_WIN32 := ws2_32 advapi32 user32

SRCS += main.cc
SRCS += exServer.cc
SRCS += exPV.cc
SRCS += exVectorPV.cc
SRCS += exScalarPV.cc
SRCS += exAsyncPV.cc
SRCS += exChannel.cc

PROD_DEFAULT := excas
PROD_vxWorks :=

include $(TOP)/configure/RULES_BUILD

clean::
	@$(RM) excas 
	@$(RM) -rf Templates.DB
	@$(RM) core 

