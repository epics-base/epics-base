TOP=../..
include $(TOP)/configure/CONFIG

INC += asLib.h
INC += asDbLib.h
INC += asCa.h
INC += asTrapWrite.h

LIBSRCS += asLib.c

asIoc_SRCS += asDbLib.c
asIoc_SRCS += asCa.c
asIoc_SRCS += asTrapWrite.c

LIBRARY_HOST = asHost
LIBRARY_IOC = asIoc

asHost_DLL_LIBS  = dbStaticHost ca Com
asIoc_DLL_LIBS  = dbIoc dbStaticIoc ca Com

asHost_DIR=.
PROD_LIBS = asHost dbStaticHost Com
PROD_HOST = ascheck
ascheck_SRCS = ascheck.c

asHost_RCS_WIN32 = asHost.rc
asIoc_RCS_WIN32 = asIoc.rc

# For R3.13 compatibility only
OBJLIB_vxWorks=asIoc
OBJLIB_SRCS = $(LIBSRCS) $(asIoc_SRCS)

include $(TOP)/configure/RULES

# Extra rule since asLib_lex.c is included in asLib.c
#   In my opinion, these objects should really be built
#   independently.
asLib.c: asLib_lex.c ../asLibRoutines.c

$(INSTALL_INC)/asLib.h:../asLib.h

clean::
	@$(RM) asLib.c asLib_lex.c 

