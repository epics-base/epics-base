
TOP=../../../..

include $(TOP)/configure/CONFIG

CXXCMPLR = STRICT

PROD_LIBS := cas ca gdd Com
cas_DIR = $(INSTALL_LIB)
ca_DIR = $(INSTALL_LIB)
gdd_DIR = $(INSTALL_LIB)
Com_DIR = $(INSTALL_LIB)

#
# Added winmm user32 for the non-dll build
#
SYS_PROD_LIBS_WIN32 := ws2_32 advapi32 user32

SRCS += main.cc
SRCS += directoryServer.cc

PROD = caDirServ

include $(TOP)/configure/RULES

pcaDirServ: $(PROD_OBJS) $(PRODDEPLIBS)
	$(PURIFY) $(PROD_LINKER) $(PROD_OBJS) $(LDLIBS)

clean::
	@$(RM) caDirServ
	@$(RM) pcaDirServ
	@$(RM) -rf Templates.DB
	@$(RM) core 

