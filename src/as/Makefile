TOP=../..
include $(TOP)/configure/CONFIG

SRCS += asLib.c
asLibrary_SRCS += asDbLib.c
asLibrary_SRCS += asCa.c

LIBRARY_Host += As
LIBRARY_Ioc += asLibrary

#	All systems link the libs As, Com, Db (and again Com ??),
#	generic Unix needs also lib m:
#
SYS_PROD_LIBS_DEFAULT := m
SYS_PROD_LIBS_WIN32   := -nil-
PROD_LIBS         := As Com Db Com

PROD = ascheck

include $(TOP)/configure/RULES

# Extra rule since asLib_lex.c is included in asLib.c
#   In my opinion, these objects should really be built
#   independently.
asLib$(OBJ): asLib_lex.c ../asLibRoutines.c

clean::
	@$(RM) asLib.c asLib_lex.c 

