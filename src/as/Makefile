TOP=../..
include $(TOP)/configure/CONFIG

LIBSRCS += asLib.c
asLibrary_SRCS += asDbLib.c
asLibrary_SRCS += asCa.c

LIBRARY_HOST = As
LIBRARY_IOC = as
DLL_LIBS  = Db Com

PROD_LIBS = As dbStatic Com
As_DIR=.

PROD = ascheck

include $(TOP)/configure/RULES

# Extra rule since asLib_lex.c is included in asLib.c
#   In my opinion, these objects should really be built
#   independently.
asLib$(OBJ): asLib_lex.c ../asLibRoutines.c

clean::
	@$(RM) asLib.c asLib_lex.c 

