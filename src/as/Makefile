TOP=../..
include $(TOP)/configure/CONFIG

INC += asDbLib.h
INC += asLib.h

LIBSRCS += asLib.c

asIoc_SRCS += asDbLib.c
asIoc_SRCS += asCa.c

LIBRARY_HOST = asHost
LIBRARY_IOC = asIoc

asHost_DLL_LIBS  = dbStaticHost Com
asIoc_DLL_LIBS  = dbStaticIoc Com

asHost_DIR=.
PROD_LIBS = asHost dbStaticHost Com
PROD = ascheck

include $(TOP)/configure/RULES

# Extra rule since asLib_lex.c is included in asLib.c
#   In my opinion, these objects should really be built
#   independently.
asLib$(OBJ): asLib_lex.c ../asLibRoutines.c

clean::
	@$(RM) asLib.c asLib_lex.c 

