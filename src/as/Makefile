
TOP=../..

include $(TOP)/configure/CONFIG

USR_CFLAGS += -D_NO_PROTO

#INC += asDbLib.h moved header to src/include
#INC += asLib.h moved header to src/include

#	build lib As from asLib.c:
#
LIBRARY_DEFAULT := As
As_SRCS := asLib.c

#	All systems link the libs As, Com, Db (and again Com ??),
#	generic Unix needs also lib m:
#
SYS_PROD_LIBS_DEFAULT := m
SYS_PROD_LIBS_WIN32   := -nil-
PROD_LIBS         := As Com Db Com

PROD_DEFAULT := ascheck
PROD_vxWorks = -nil-

LIBRARY_vxWorks = asLibrary

asLibrary_CFLAGS = -DACCESS_SECURITY -D_NO_PROTO

asLibrary_SRCS = asDbLib.c asCa.c asLib.c

include $(TOP)/configure/RULES_BUILD

# Extra rule since asLib_lex.c is included in asLib.c
#   In my opinion, these objects should really be built
#   independently.
asLib$(OBJ): asLib_lex.c ../asLibRoutines.c

clean::
	@$(RM) asLib.c asLib_lex.c 

