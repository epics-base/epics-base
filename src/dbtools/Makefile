TOP=../..

include $(TOP)/configure/CONFIG

#YACCOPT := -l
#LEXOPT  := -L

INC += dbLoadTemplate.h

LIBSRCS += dbLoadTemplate.c

LIBRARY_IOC = dbtoolsIoc

dbtoolsIoc_DLL_LIBS  = dbIoc Com

HTMLS += dbLoadTemplate.html

dbtoolsIoc_RCS_WIN32 = dbtoolsIoc.rc

include $(TOP)/configure/RULES
 
#
#	These lex sources are included in some C sources,
#	so they have to be created in time:
#
dbLoadTemplate$(OBJ): dbLoadTemplate_lex.c ../dbLoadTemplate.h

clean::
	@$(RM) dbLoadTemplate_lex.c dbLoadTemplate.c 

