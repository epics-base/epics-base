TOP=../..

include $(TOP)/configure/CONFIG

#YACCOPT := -l
#LEXOPT  := -L

LIBSRCS += dbLoadTemplate.c

LIBRARY_IOC = dbtoolsIoc

dbtoolsIoc_DLL_LIBS  = dbStaticIoc Com

HTMLS += dbLoadTemplate.html

include $(TOP)/configure/RULES
 
#
#	These lex sources are included in some C sources,
#	so they have to be created in time:
#
dbLoadTemplate$(OBJ): dbLoadTemplate_lex.c

clean::
	@$(RM) dbLoadTemplate_lex.c dbLoadTemplate.c 

